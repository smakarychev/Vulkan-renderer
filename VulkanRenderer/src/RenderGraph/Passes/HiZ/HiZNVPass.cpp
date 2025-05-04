#include "HiZNVPass.h"

#include "HiZBlitPass.h"
#include "RenderGraph/RenderGraph.h"
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
        .Subresource = info.Subresource,
        .ReductionMode = info.ReductionMode,
        .CalculateMinMax = true});
    
    const u32 mipmapCount = (u32)(u8)renderGraph.GetTextureDescription(blit.HiZ).Mipmaps;
    const glm::uvec2 hizResolution = renderGraph.GetTextureDescription(blit.HiZ).Dimensions();
    u32 width = hizResolution.x;  
    u32 height = hizResolution.y;

    u32 mipmapsRemaining = mipmapCount - 1;
    u32 currentMipmap = 0;

    while (mipmapsRemaining != 0)
    {
        u32 toBeProcessed = std::min(MAX_DISPATCH_MIPMAPS, mipmapsRemaining);

        PassData& data = renderGraph.AddRenderPass<PassData>(name.AddVersion(currentMipmap),
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZNV.Setup")

                graph.SetShader("hiz-nv"_hsv);

                passData.Depth = graph.Read(blit.Depth, Compute | Sampled);
                passData.HiZ = graph.Write(blit.HiZ, Compute | Storage);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZNV")
                GPU_PROFILE_FRAME("HiZNV")

                Texture hiz = resources.GetTexture(passData.HiZ);
                Span hizViews = Device::GetAdditionalImageViews(hiz);
                
                const Shader& shader = resources.GetGraph()->GetShader();
                HizNvShaderBindGroup bindGroup(shader);
                bindGroup.SetInSampler(HiZ::createSampler(info.ReductionMode));
                bindGroup.SetInImage({.Image = hiz}, ImageLayout::General);

                for (u32 i = 0; i < hizViews.size(); i++)
                    bindGroup.SetHizMips({
                        .Image = hiz,
                        .Description = hizViews[i]}, ImageLayout::General, i);

                u32 pushConstant = currentMipmap << MIPMAP_LEVEL_SHIFT | toBeProcessed;
                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {pushConstant}});
                u32 shift = toBeProcessed > 5 ? 12 : 10;
                u32 mask = toBeProcessed > 5 ? 4095 : 1023;
                u32 samples = width * height;
                cmd.Dispatch({
                    .Invocations = {(samples + mask) >> shift, 1, 1}});
            }).Data;

        width = std::max(1u, width >> toBeProcessed);
        height = std::max(1u, height >> toBeProcessed);
        currentMipmap += toBeProcessed;
        mipmapsRemaining -= toBeProcessed;

        if (mipmapsRemaining == 0)
            return data;
    }

    std::unreachable();
}