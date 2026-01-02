#include "rendererpch.h"

#include "HiZPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/HizBlitBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/HizGenerateBindGroupRG.generated.h"

namespace
{
    struct BlitPassData
    {
        RG::Resource Depth{};
        RG::Resource HiZ{};
        RG::Resource DepthMinMax{};
        std::array<RG::Resource, HiZ::MAX_MIP_LEVELS> HiZMips{};
        HizBlitBindGroupRG BindGroup{};
    };

    BlitPassData& blitPass(StringId name, RG::Graph& renderGraph, const Passes::HiZ::ExecutionInfo& info)
    {
        using namespace RG;

        return renderGraph.AddRenderPass<BlitPassData>(name,
            [&](Graph& graph, BlitPassData& passData)
            {
                CPU_PROFILE_FRAME("HiZCombined.Blit.Setup")

                passData.BindGroup = HizBlitBindGroupRG(graph, ShaderOverrides{
                    ShaderDefines({
                        ShaderDefine("HIZ_BLIT"_hsv, true),
                        ShaderDefine("HIZ_GENERATE"_hsv, false),
                        ShaderDefine("HIZ_MIN_MAX"_hsv, info.ReductionMode == HiZ::ReductionMode::MinMax),
                        ShaderDefine("HIZ_MIN_MAX_DEPTH_BUFFER"_hsv, info.CalculateMinMaxDepthBuffer),
                    })
                });

                passData.HiZ = HiZ::createHiz(graph, graph.GetImageDescription(info.Depth).Dimensions(),
                    info.ReductionMode);
                passData.HiZMips[0] = graph.SplitImage(passData.HiZ, {.MipmapBase = 0, .Mipmaps = 1});

                if (info.CalculateMinMaxDepthBuffer)
                {
                    passData.DepthMinMax = passData.BindGroup.SetResourcesMinMax(
                        HiZ::createMinMaxBufferResource(graph));
                    passData.DepthMinMax = graph.Upload(passData.DepthMinMax, HiZ::MinMaxDepth{});
                }
                
                passData.Depth = passData.BindGroup.SetResourcesInput(info.Depth);
                passData.HiZMips[0] = passData.BindGroup.SetResourcesOutput(passData.HiZMips[0], 0);
            },
            [=](const BlitPassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("HiZCombined.Blit")
                GPU_PROFILE_FRAME("HiZCombined.Blit")

                const glm::uvec2 hizResolution = graph.GetImageDescription(passData.HiZ).Dimensions();

                auto& cmd = frameContext.CommandList;
                passData.BindGroup.BindCompute(frameContext.CommandList, graph.GetFrameAllocators());
                cmd.Dispatch({
                    .Invocations = {hizResolution.x, hizResolution.y, 1},
                    .GroupSize = passData.BindGroup.GetBlitPassGroupSize()
                });
            });
    }
}

Passes::HiZ::PassData& Passes::HiZ::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5;

    using namespace RG;

    auto& blit = blitPass("HiZCombined.Blit"_hsv, renderGraph, info);

    const u32 mipmapCount = (u32)(u8)renderGraph.GetImageDescription(blit.HiZ).Mipmaps;
    const glm::uvec2 hizResolution = renderGraph.GetImageDescription(blit.HiZ).Dimensions();
    u32 width = hizResolution.x;
    u32 height = hizResolution.y;

    u32 mipmapsRemaining = mipmapCount - 1;
    u32 currentMipmap = 0;

    for (i8 mipmap = 1; mipmap < (i8)mipmapCount; mipmap++)
        blit.HiZMips[mipmap] = renderGraph.SplitImage(blit.HiZ, {.MipmapBase = mipmap, .Mipmaps = 1});

    using PassDataBind = PassDataWithBind<PassData, HizGenerateBindGroupRG>;

    while (mipmapsRemaining != 0)
    {
        u32 toBeProcessed = std::min(MAX_DISPATCH_MIPMAPS, mipmapsRemaining);
        PassData& data = renderGraph.AddRenderPass<PassDataBind>(name.AddVersion(currentMipmap),
            [&](Graph& graph, PassDataBind& passData)
            {
                CPU_PROFILE_FRAME("HiZCombined.Setup")

                passData.BindGroup = HizGenerateBindGroupRG(graph, ShaderOverrides{
                    ShaderDefines({
                         ShaderDefine("HIZ_BLIT"_hsv, false),
                         ShaderDefine("HIZ_GENERATE"_hsv, true),
                         ShaderDefine("HIZ_MIN_MAX"_hsv, info.ReductionMode == ::HiZ::ReductionMode::MinMax),
                         ShaderDefine("HIZ_MIN_MAX_DEPTH_BUFFER"_hsv, info.CalculateMinMaxDepthBuffer),
                     })
                });

                const u32 sourceMipmapIndex = currentMipmap;
                blit.HiZMips[sourceMipmapIndex] = passData.BindGroup.SetResourcesInput(blit.HiZMips[sourceMipmapIndex]);

                for (u32 i = 0; i < toBeProcessed + 1; i++)
                {
                    u32 index = sourceMipmapIndex + i;
                    if (index >= mipmapCount)
                        break;

                    blit.HiZMips[index] = passData.BindGroup.SetResourcesOutput(blit.HiZMips[index], index);
                }
                    
                if (mipmapsRemaining - toBeProcessed == 0)
                    blit.HiZ = graph.MergeImage(Span<const Resource>(blit.HiZMips.data(), mipmapCount));
                passData.HiZ = blit.HiZ;
                passData.DepthMinMax = blit.DepthMinMax;
            },
            [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("HiZCombined")
                GPU_PROFILE_FRAME("HiZCombined")

                const u32 pushConstant = currentMipmap << MIPMAP_LEVEL_SHIFT | toBeProcessed;
                auto& cmd = frameContext.CommandList;
                passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
                cmd.PushConstants({
                    .PipelineLayout = passData.BindGroup.Shader->GetLayout(),
                    .Data = {pushConstant}
                });
                const u32 shift = toBeProcessed > 5 ? 12 : 10;
                const u32 mask = toBeProcessed > 5 ? 4095 : 1023;
                const u32 samples = width * height;
                cmd.Dispatch({
                    .Invocations = {(samples + mask) >> shift, 1, 1}
                });
            });

        width = std::max(1u, width >> toBeProcessed);
        height = std::max(1u, height >> toBeProcessed);
        currentMipmap += toBeProcessed;
        mipmapsRemaining -= toBeProcessed;

        if (mipmapsRemaining == 0)
            return data;
    }

    std::unreachable();
}
