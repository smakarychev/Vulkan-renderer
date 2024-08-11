#include "HiZFullPass.h"

#include "HiZBlitUtilityPass.h"
#include "HiZPassContext.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::HiZFull::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
    ImageSubresourceDescription::Packed subresource, HiZPassContext& ctx)
{
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5;

    using namespace RG;
    using enum ResourceAccessFlags;

    u32 mipmapCount = ctx.GetHiZ(HiZReductionMode::Min).Description().Mipmaps;
    u32 width = ctx.GetHiZ(HiZReductionMode::Min).Description().Width;  
    u32 height = ctx.GetHiZ(HiZReductionMode::Min).Description().Height;

    auto& minBlit = HiZBlit::addToGraph(std::format("{}.BlitMin", name), renderGraph, depth, subresource, ctx,
        HiZReductionMode::Min, true);
    HiZBlit::addToGraph(std::format("{}.BlitMax", name), renderGraph, depth, subresource, ctx,
        HiZReductionMode::Max);

    u32 mipmapsRemaining = mipmapCount - 1;
    u32 currentMipmap = 0;
    
    while (mipmapsRemaining != 0)
    {
        u32 toBeProcessed = std::min(MAX_DISPATCH_MIPMAPS, mipmapsRemaining);
        Pass& pass = renderGraph.AddRenderPass<PassData>(PassName{std::format("{}.{}", name, currentMipmap)},
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZFull.Setup")

                graph.SetShader("../assets/shaders/hiz-full.shader");

                passData.DepthMin = graph.Read(ctx.GetHiZResource(HiZReductionMode::Min), Compute | Sampled);
                passData.HiZMinOut = graph.Write(ctx.GetHiZResource(HiZReductionMode::Min), Compute | Storage);
                passData.DepthMax = graph.Read(ctx.GetHiZResource(HiZReductionMode::Max), Compute | Sampled);
                passData.HiZMaxOut = graph.Write(ctx.GetHiZResource(HiZReductionMode::Max), Compute | Storage);

                passData.MinSampler = ctx.GetMinMaxSampler(HiZReductionMode::Min);
                passData.MaxSampler = ctx.GetMinMaxSampler(HiZReductionMode::Max);
                passData.MipmapViewHandles = ctx.GetViewHandles();

                passData.MinMaxDepth = graph.GetBlackboard().Get<HiZBlit::PassData>(minBlit).MinMaxDepth;
                
                ctx.SetHiZResource(passData.HiZMinOut, HiZReductionMode::Min);
                ctx.SetHiZResource(passData.HiZMaxOut, HiZReductionMode::Max);
                
                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZFull")
                GPU_PROFILE_FRAME("HiZFull")

                const Texture hizMinOutput = resources.GetTexture(passData.HiZMinOut);
                const Texture hizMaxOutput = resources.GetTexture(passData.HiZMaxOut);

                TextureBindingInfo hizMinInput = hizMinOutput.BindingInfo(
                    passData.MinSampler, ImageLayout::General);
                TextureBindingInfo hizMaxInput = hizMaxOutput.BindingInfo(
                    passData.MaxSampler, ImageLayout::General);

                const Shader& shader = resources.GetGraph()->GetShader();
                auto& pipeline = shader.Pipeline(); 
                auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);
                
                samplerDescriptors.UpdateBinding("u_min_sampler", hizMinInput);
                samplerDescriptors.UpdateBinding("u_max_sampler", hizMaxInput);
                resourceDescriptors.UpdateBinding("u_min_image", hizMinInput);
                resourceDescriptors.UpdateBinding("u_max_image", hizMaxInput);
                
                for (u32 i = 0; i < passData.MipmapViewHandles.size(); i++)
                {
                    resourceDescriptors.UpdateBinding("u_output_min",
                        resources.GetTexture(passData.HiZMinOut).BindingInfo(
                            passData.MinSampler, ImageLayout::General, passData.MipmapViewHandles[i]), i);
                    resourceDescriptors.UpdateBinding("u_output_max",
                        resources.GetTexture(passData.HiZMaxOut).BindingInfo(
                            passData.MaxSampler, ImageLayout::General, passData.MipmapViewHandles[i]), i);
                }

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
