#include "HiZNVPass.h"

#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

namespace
{
    void addBlitPass(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
        ImageSubresourceDescription::Packed subresource, HiZPassContext& ctx)
    {
        using PassData = Passes::HiZNV::PassData;
        using namespace RG;
        using enum ResourceAccessFlags;

        u32 width = ctx.GetHiZ().Description().Width;  
        u32 height = ctx.GetHiZ().Description().Height;

        renderGraph.AddRenderPass<PassData>(PassName{std::format("{}.Blit", name)},
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZNV.Blit.Setup")

                graph.SetShader("../assets/shaders/hiz.shader");
                
                Resource depthIn = depth;
                Resource depthOut = graph.AddExternal("Hiz.Out", ctx.GetHiZ());
                graph.Export(depthOut, ctx.GetHiZPrevious(), true);
                
                passData.DepthIn = graph.Read(depthIn, Compute | Sampled);
                passData.HiZOut = graph.Write(depthOut, Compute | Storage);

                passData.MinMaxSampler = ctx.GetSampler();
                passData.MipmapViewHandles = ctx.GetViewHandles();

                ctx.SetHiZResource(passData.HiZOut);
                
                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZNV.Blit")
                GPU_PROFILE_FRAME("HiZNV.Blit")
                
                const Texture& depthIn = resources.GetTexture(passData.DepthIn);
                const Texture& hizOut = resources.GetTexture(passData.HiZOut);
                
                TextureBindingInfo depthInBinding = depthIn.BindingInfo(passData.MinMaxSampler,
                    ImageLayout::DepthReadonly, depthIn.GetViewHandle(subresource));

                const Shader& shader = resources.GetGraph()->GetShader();
                auto& pipeline = shader.Pipeline(); 
                auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);
                
                samplerDescriptors.UpdateBinding("u_in_sampler", depthInBinding);
                resourceDescriptors.UpdateBinding("u_in_image", depthInBinding);
                resourceDescriptors.UpdateBinding("u_out_image",
                    hizOut.BindingInfo(
                        passData.MinMaxSampler, ImageLayout::General, passData.MipmapViewHandles[0]));
                
                glm::uvec2 levels = {width, height};
                pipeline.BindCompute(frameContext.Cmd);
                RenderCommand::PushConstants(frameContext.Cmd, pipeline.GetLayout(), levels);
                samplerDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                    pipeline.GetLayout());
                resourceDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                    pipeline.GetLayout());
                RenderCommand::Dispatch(frameContext.Cmd, {(width + 32 - 1) / 32, (height + 32 - 1) / 32, 1});
            });
    }
}

RG::Pass& Passes::HiZNV::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
    ImageSubresourceDescription::Packed subresource, HiZPassContext& ctx)
{
    // https://github.com/nvpro-samples/vk_compute_mipmaps
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5; 
    
    using namespace RG;
    using enum ResourceAccessFlags;

    u32 mipmapCount = ctx.GetHiZ().Description().Mipmaps;
    u32 width = ctx.GetHiZ().Description().Width;  
    u32 height = ctx.GetHiZ().Description().Height;

    /* first we have to blit the depth onto the hiz texture using special sampler,
     * it cannot be done by api call, and we have to use a compute shader for that
     * todo: it is possible to change nvpro shader to do that
     */
    addBlitPass(name, renderGraph, depth, subresource, ctx);
    
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

                Resource depthIn = ctx.GetHiZResource(); 
                Resource depthOut = ctx.GetHiZResource();

                passData.DepthIn = graph.Read(depthIn, Compute | Sampled);
                passData.HiZOut = graph.Write(depthOut, Compute | Storage);

                passData.MinMaxSampler = ctx.GetSampler();
                passData.MipmapViewHandles = ctx.GetViewHandles();

                ctx.SetHiZResource(passData.HiZOut);
                
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
