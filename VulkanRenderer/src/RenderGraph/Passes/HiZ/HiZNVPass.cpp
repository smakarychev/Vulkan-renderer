#include "HiZNVPass.h"

#include "HiZBlitUtilityPass.h"
#include "HiZPassContext.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

RG::Pass& Passes::HiZNV::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
    ImageSubresourceDescription::Packed subresource, HiZPassContext& ctx)
{
    /* https://github.com/nvpro-samples/vk_compute_mipmaps */
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5; 
    
    using namespace RG;
    using enum ResourceAccessFlags;

    u32 mipmapCount = ctx.GetHiZ(HiZReductionMode::Min).Description().Mipmaps;
    u32 width = ctx.GetHiZ(HiZReductionMode::Min).Description().Width;  
    u32 height = ctx.GetHiZ(HiZReductionMode::Min).Description().Height;

    /* first we have to blit the depth onto the hiz texture using special sampler,
     * it cannot be done by api call, and we have to use a compute shader for that
     * todo: it is possible to change nvpro shader to do that
     */
    HiZBlit::addToGraph<PassData>(name, renderGraph, depth, subresource, ctx, HiZReductionMode::Min);
    
    u32 mipmapsRemaining = mipmapCount - 1;
    u32 currentMipmap = 0;

    while (mipmapsRemaining != 0)
    {
        u32 toBeProcessed = std::min(MAX_DISPATCH_MIPMAPS, mipmapsRemaining);

        Pass& pass = renderGraph.AddRenderPass<PassData>(PassName{std::format("{}.{}", name, currentMipmap)},
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZNV.Setup")

                graph.SetShader("../assets/shaders/hiz-nv.shader");

                passData.DepthIn = graph.Read(ctx.GetHiZResource(HiZReductionMode::Min), Compute | Sampled);
                passData.HiZOut = graph.Write(ctx.GetHiZResource(HiZReductionMode::Min), Compute | Storage);

                passData.MinMaxSampler = ctx.GetMinMaxSampler(HiZReductionMode::Min);
                passData.MipmapViewHandles = ctx.GetViewHandles();

                ctx.SetHiZResource(passData.HiZOut, HiZReductionMode::Min);
                
                graph.UpdateBlackboard(passData);

            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZNV")
                GPU_PROFILE_FRAME("HiZNV")

                const Texture hizOutput = resources.GetTexture(passData.HiZOut);

                TextureBindingInfo hizInput = resources.GetTexture(passData.HiZOut).BindingInfo(
                    passData.MinMaxSampler, ImageLayout::General);
                
                const Shader& shader = resources.GetGraph()->GetShader();
                auto& pipeline = shader.Pipeline(); 
                auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);
                
                samplerDescriptors.UpdateBinding("u_in_sampler", hizInput);
                resourceDescriptors.UpdateBinding("u_in_image", hizInput);

                for (u32 i = 0; i < passData.MipmapViewHandles.size(); i++)
                    resourceDescriptors.UpdateBinding("u_hiz_mips",
                        resources.GetTexture(passData.HiZOut).BindingInfo(
                            passData.MinMaxSampler, ImageLayout::General, passData.MipmapViewHandles[i]), i);

                u32 pushConstant = currentMipmap << MIPMAP_LEVEL_SHIFT | toBeProcessed;
                pipeline.BindCompute(frameContext.Cmd);
                RenderCommand::PushConstants(frameContext.Cmd, pipeline.GetLayout(), pushConstant);
                samplerDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                    pipeline.GetLayout());
                resourceDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                    pipeline.GetLayout());
                u32 shift = toBeProcessed > 5 ? 12 : 10;
                u32 mask = toBeProcessed > 5 ? 4095 : 1023;
                u32 samples = width * height;
                RenderCommand::Dispatch(frameContext.Cmd, {(samples + mask) >> shift, 1, 1});
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
