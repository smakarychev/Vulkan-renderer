#include "HiZNVPass.h"

#include "HiZBlitPass.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/HizNvBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

Passes::HiZNV::PassData& Passes::HiZNV::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    /* https://github.com/nvpro-samples/vk_compute_mipmaps */
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5; 
    
    using namespace RG;
    using enum ResourceAccessFlags;

    /* first we have to blit the depth onto the hiz texture using special sampler,
     * it cannot be done by api call, and we have to use a compute shader for that
     * todo: it is possible to change nvpro shader to do that
     */
    auto& blit = HiZBlit::addToGraph(name.Concatenate(".BlitMin"), renderGraph, {
        .Depth = info.Depth,
        .ReductionMode = info.ReductionMode,
        .CalculateMinMax = true});
    
    const u32 mipmapCount = (u32)(u8)renderGraph.GetImageDescription(blit.HiZ).Mipmaps;
    const glm::uvec2 hizResolution = renderGraph.GetImageDescription(blit.HiZ).Dimensions();
    u32 width = hizResolution.x;  
    u32 height = hizResolution.y;

    u32 mipmapsRemaining = mipmapCount - 1;
    u32 currentMipmap = 0;

    std::array hizMips = blit.HiZMips;

    for (i8 mipmap = 1; mipmap < (i8)mipmapCount; mipmap++)
        hizMips[mipmap] = renderGraph.SplitImage(blit.HiZ, {.MipmapBase = mipmap, .Mipmaps = 1});

    while (mipmapsRemaining != 0)
    {
        u32 toBeProcessed = std::min(MAX_DISPATCH_MIPMAPS, mipmapsRemaining);

        PassData& data = renderGraph.AddRenderPass<PassData>(name.AddVersion(currentMipmap),
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZNV.Setup")

                graph.SetShader("hiz-nv"_hsv);

                passData.Depth = graph.ReadImage(blit.Depth, Compute | Sampled);
                for (u32 i = currentMipmap; i < currentMipmap + toBeProcessed; i++)
                    hizMips[i] = graph.ReadWriteImage(hizMips[i], Compute | Sampled);
                if (mipmapsRemaining - toBeProcessed == 0)
                    blit.HiZ = graph.MergeImage(Span<const Resource>(hizMips.data(), mipmapCount));
                passData.HiZ = blit.HiZ;
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("HiZNV")
                GPU_PROFILE_FRAME("HiZNV")

                const Shader& shader = graph.GetShader();
                HizNvShaderBindGroup bindGroup(shader);
                bindGroup.SetInSampler(HiZ::createSampler(info.ReductionMode));
                bindGroup.SetInImage(graph.GetImageBinding(passData.HiZ));

                for (u32 i = 0; i < mipmapCount; i++)
                    bindGroup.SetHizMips(graph.GetImageBinding(hizMips[i]), i);

                u32 pushConstant = currentMipmap << MIPMAP_LEVEL_SHIFT | toBeProcessed;
                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
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