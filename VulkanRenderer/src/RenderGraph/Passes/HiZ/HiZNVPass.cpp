#include "HiZNVPass.h"

#include "HiZBlitUtilityPass.h"
#include "HiZPassContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/HizNvBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::HiZNV::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource depth,
    ImageSubresourceDescription subresource, HiZPassContext& ctx)
{
    /* https://github.com/nvpro-samples/vk_compute_mipmaps */
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5; 
    
    using namespace RG;
    using enum ResourceAccessFlags;

    const TextureDescription& hizDescription = Device::GetImageDescription(ctx.GetHiZ(HiZReductionMode::Min));
    u32 mipmapCount = (u32)hizDescription.Mipmaps;
    u32 width = hizDescription.Width;  
    u32 height = hizDescription.Height;

    /* first we have to blit the depth onto the hiz texture using special sampler,
     * it cannot be done by api call, and we have to use a compute shader for that
     * todo: it is possible to change nvpro shader to do that
     */
    HiZBlit::addToGraph(name, renderGraph, depth, subresource, ctx, HiZReductionMode::Min);
    
    u32 mipmapsRemaining = mipmapCount - 1;
    u32 currentMipmap = 0;

    while (mipmapsRemaining != 0)
    {
        u32 toBeProcessed = std::min(MAX_DISPATCH_MIPMAPS, mipmapsRemaining);

        Pass& pass = renderGraph.AddRenderPass<PassData>(name.AddVersion(currentMipmap),
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZNV.Setup")

                graph.SetShader("hiz-nv.shader");

                passData.DepthIn = graph.Read(ctx.GetHiZResource(HiZReductionMode::Min), Compute | Sampled);
                passData.HiZOut = graph.Write(ctx.GetHiZResource(HiZReductionMode::Min), Compute | Storage);

                passData.MinMaxSampler = ctx.GetMinMaxSampler(HiZReductionMode::Min);
                passData.MipmapViews = ctx.GetViewHandles();

                ctx.SetHiZResource(passData.HiZOut, HiZReductionMode::Min);
                
                graph.UpdateBlackboard(passData);

            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZNV")
                GPU_PROFILE_FRAME("HiZNV")

                Texture hizOutput = resources.GetTexture(passData.HiZOut);
                
                const Shader& shader = resources.GetGraph()->GetShader();
                HizNvShaderBindGroup bindGroup(shader);
                bindGroup.SetInSampler(passData.MinMaxSampler);
                bindGroup.SetInImage({.Image = hizOutput}, ImageLayout::General);

                for (u32 i = 0; i < passData.MipmapViews.size(); i++)
                    bindGroup.SetHizMips({
                        .Image = hizOutput,
                        .Description = passData.MipmapViews[i]}, ImageLayout::General, i);

                u32 pushConstant = currentMipmap << MIPMAP_LEVEL_SHIFT | toBeProcessed;
                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
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
            return pass;
    }

    std::unreachable();
}
