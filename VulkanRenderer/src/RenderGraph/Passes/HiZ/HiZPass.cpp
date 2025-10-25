#include "rendererpch.h"

#include "HiZPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/HizCombinedBindGroup.generated.h"

namespace
{
    struct BlitPassData
    {
        RG::Resource Depth{};
        RG::Resource HiZ{};
        RG::Resource DepthMinMax{};
        std::array<RG::Resource, HiZ::MAX_MIP_LEVELS> HiZMips{};
    };

    BlitPassData& blitPass(StringId name, RG::Graph& renderGraph, const Passes::HiZ::ExecutionInfo& info)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        return renderGraph.AddRenderPass<BlitPassData>(name,
            [&](Graph& graph, BlitPassData& passData)
            {
                CPU_PROFILE_FRAME("HiZCombined.Blit.Setup")

                graph.SetShader("hiz-combined"_hsv, ShaderOverrides{
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
                    passData.DepthMinMax = HiZ::createMinMaxBufferResource(graph);
                    passData.DepthMinMax = graph.ReadWriteBuffer(passData.DepthMinMax, Compute | Storage);
                    passData.DepthMinMax = graph.Upload(passData.DepthMinMax, HiZ::MinMaxDepth{});
                }

                passData.Depth = graph.ReadImage(info.Depth, Compute | Sampled);
                passData.HiZMips[0] = graph.WriteImage(passData.HiZMips[0], Compute | Storage);
            },
            [=](const BlitPassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("HiZCombined.Blit")
                GPU_PROFILE_FRAME("HiZCombined.Blit")

                const glm::uvec2 hizResolution = graph.GetImageDescription(passData.HiZ).Dimensions();

                const Shader& shader = graph.GetShader();
                HizCombinedShaderBindGroup bindGroup(shader);
                bindGroup.SetInput(graph.GetImageBinding(passData.Depth));
                bindGroup.SetOutput(graph.GetImageBinding(passData.HiZMips[0]), 0);
                if (info.CalculateMinMaxDepthBuffer)
                    bindGroup.SetMinMax(graph.GetBufferBinding(passData.DepthMinMax));

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
                cmd.Dispatch({
                    .Invocations = {hizResolution.x, hizResolution.y, 1},
                    .GroupSize = {8, 8, 1}
                });
            });
    }
}

Passes::HiZ::PassData& Passes::HiZ::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5;

    using namespace RG;
    using enum ResourceAccessFlags;

    auto& blit = blitPass("HiZCombined.Blit"_hsv, renderGraph, info);

    const u32 mipmapCount = (u32)(u8)renderGraph.GetImageDescription(blit.HiZ).Mipmaps;
    const glm::uvec2 hizResolution = renderGraph.GetImageDescription(blit.HiZ).Dimensions();
    u32 width = hizResolution.x;
    u32 height = hizResolution.y;

    u32 mipmapsRemaining = mipmapCount - 1;
    u32 currentMipmap = 0;

    for (i8 mipmap = 1; mipmap < (i8)mipmapCount; mipmap++)
        blit.HiZMips[mipmap] = renderGraph.SplitImage(blit.HiZ, {.MipmapBase = mipmap, .Mipmaps = 1});

    while (mipmapsRemaining != 0)
    {
        u32 toBeProcessed = std::min(MAX_DISPATCH_MIPMAPS, mipmapsRemaining);
        PassData& data = renderGraph.AddRenderPass<PassData>(name.AddVersion(currentMipmap),
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZCombined.Setup")

                graph.SetShader("hiz-combined"_hsv, ShaderOverrides{
                    ShaderDefines({
                        ShaderDefine("HIZ_BLIT"_hsv, false),
                        ShaderDefine("HIZ_GENERATE"_hsv, true),
                        ShaderDefine("HIZ_MIN_MAX"_hsv, info.ReductionMode == ::HiZ::ReductionMode::MinMax),
                        ShaderDefine("HIZ_MIN_MAX_DEPTH_BUFFER"_hsv, info.CalculateMinMaxDepthBuffer),
                    })
                });

                const u32 sourceMipmapIndex = currentMipmap;
                blit.HiZMips[sourceMipmapIndex] = graph.ReadImage(blit.HiZMips[sourceMipmapIndex], Compute | Sampled);
                
                for (u32 i = 0; i < toBeProcessed; i++)
                    blit.HiZMips[sourceMipmapIndex + i + 1] =
                        graph.ReadWriteImage(blit.HiZMips[sourceMipmapIndex + i + 1], Compute | Sampled);
                if (mipmapsRemaining - toBeProcessed == 0)
                    blit.HiZ = graph.MergeImage(Span<const Resource>(blit.HiZMips.data(), mipmapCount));
                passData.HiZ = blit.HiZ;
                passData.DepthMinMax = blit.DepthMinMax;
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("HiZCombined")
                GPU_PROFILE_FRAME("HiZCombined")

                const Shader& shader = graph.GetShader();
                HizCombinedShaderBindGroup bindGroup(shader);
                bindGroup.SetInput(graph.GetImageBinding(passData.HiZ));
                if (info.CalculateMinMaxDepthBuffer)
                    bindGroup.SetMinMax(graph.GetBufferBinding(passData.DepthMinMax));
                
                for (u32 i = 0; i < mipmapCount; i++)
                    bindGroup.SetOutput(graph.GetImageBinding(blit.HiZMips[i]), i);

                const u32 pushConstant = currentMipmap << MIPMAP_LEVEL_SHIFT | toBeProcessed;
                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, graph.GetFrameAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(),
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
