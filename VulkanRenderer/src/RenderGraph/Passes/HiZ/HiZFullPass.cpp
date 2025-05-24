#include "HiZFullPass.h"

#include "HiZBlitPass.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/DepthReductionBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::HiZFull::PassData& Passes::HiZFull::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5;

    using namespace RG;
    using enum ResourceAccessFlags;

    auto& minBlit = HiZBlit::addToGraph(name.Concatenate(".BlitMin"), renderGraph, {
        .Depth = info.Depth,
        .ReductionMode = HiZ::ReductionMode::Min,
        .CalculateMinMax = true});
    auto& maxBlit = HiZBlit::addToGraph(name.Concatenate(".BlitMax"), renderGraph, {
        .Depth = info.Depth,
        .ReductionMode = HiZ::ReductionMode::Max,
        .CalculateMinMax = false});

    const u32 mipmapCount = (u32)(u8)renderGraph.GetImageDescription(minBlit.HiZ).Mipmaps;
    const glm::uvec2 hizResolution = renderGraph.GetImageDescription(minBlit.HiZ).Dimensions();
    u32 width = hizResolution.x;  
    u32 height = hizResolution.y;

    u32 mipmapsRemaining = mipmapCount - 1;
    u32 currentMipmap = 0;

    std::array hiZMinMips = minBlit.HiZMips;
    std::array hiZMaxMips = maxBlit.HiZMips;

    for (i8 mipmap = 1; mipmap < (i8)mipmapCount; mipmap++)
    {
        hiZMinMips[mipmap] = renderGraph.SplitImage(minBlit.HiZ, {.MipmapBase = mipmap, .Mipmaps = 1});
        hiZMaxMips[mipmap] = renderGraph.SplitImage(maxBlit.HiZ, {.MipmapBase = mipmap, .Mipmaps = 1});
    }
    
    while (mipmapsRemaining != 0)
    {
        u32 toBeProcessed = std::min(MAX_DISPATCH_MIPMAPS, mipmapsRemaining);
        PassData& data = renderGraph.AddRenderPass<PassData>(name.AddVersion(currentMipmap),
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZFull.Setup")

                graph.SetShader("depth-reduction"_hsv);

                passData.Depth = graph.ReadImage(minBlit.Depth, Compute | Sampled);
                for (u32 i = currentMipmap; i < currentMipmap + toBeProcessed; i++)
                {
                    hiZMinMips[i] = graph.ReadWriteImage(hiZMinMips[i], Compute | Sampled);
                    hiZMaxMips[i] = graph.ReadWriteImage(hiZMaxMips[i], Compute | Sampled);
                }
                if (mipmapsRemaining - toBeProcessed == 0)
                {
                    minBlit.HiZ = graph.MergeImage(Span<const Resource>(hiZMinMips.data(), mipmapCount));
                    maxBlit.HiZ = graph.MergeImage(Span<const Resource>(hiZMaxMips.data(), mipmapCount));
                }
                passData.HiZMin = minBlit.HiZ;
                passData.HiZMax = maxBlit.HiZ;
                passData.MinMaxDepth = minBlit.MinMaxDepth;
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("HiZFull")
                GPU_PROFILE_FRAME("HiZFull")

                const Shader& shader = graph.GetShader();
                DepthReductionShaderBindGroup bindGroup(shader);
                bindGroup.SetMinSampler(HiZ::createSampler(HiZ::ReductionMode::Min));
                bindGroup.SetMaxSampler(HiZ::createSampler(HiZ::ReductionMode::Max));
                bindGroup.SetMinImage(graph.GetImageBinding(passData.HiZMin));
                bindGroup.SetMaxImage(graph.GetImageBinding(passData.HiZMax));
                
                for (u32 i = 0; i < mipmapCount; i++)
                {
                    bindGroup.SetOutputMin(graph.GetImageBinding(hiZMinMips[i]), i);
                    bindGroup.SetOutputMax(graph.GetImageBinding(hiZMaxMips[i]), i);
                }

                u32 pushConstant = currentMipmap << MIPMAP_LEVEL_SHIFT | toBeProcessed;
                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(cmd, graph.GetFrameAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {pushConstant}});
                u32 shift = toBeProcessed > 5 ? 12 : 10;
                u32 mask = toBeProcessed > 5 ? 4095 : 1023;
                u32 samples = width * height;
                cmd.Dispatch({
                    .Invocations = {(samples + mask) >> shift, 1, 1}});
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