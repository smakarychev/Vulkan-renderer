#include "HiZFullPass.h"

#include "HiZBlitUtilityPass.h"
#include "HiZPassContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/DepthReductionBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::HiZFull::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
    ImageSubresourceDescription subresource, HiZPassContext& ctx)
{
    static constexpr u32 MAX_DISPATCH_MIPMAPS = 6;
    static constexpr u32 MIPMAP_LEVEL_SHIFT = 5;

    using namespace RG;
    using enum ResourceAccessFlags;

    const TextureDescription& hizDescription = Device::GetImageDescription(ctx.GetHiZ(HiZReductionMode::Min));
    u32 mipmapCount = (u32)hizDescription.Mipmaps;
    u32 width = hizDescription.Width;  
    u32 height = hizDescription.Height;

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

                graph.SetShader("hiz-full.shader");

                passData.DepthMin = graph.Read(ctx.GetHiZResource(HiZReductionMode::Min), Compute | Sampled);
                passData.HiZMinOut = graph.Write(ctx.GetHiZResource(HiZReductionMode::Min), Compute | Storage);
                passData.DepthMax = graph.Read(ctx.GetHiZResource(HiZReductionMode::Max), Compute | Sampled);
                passData.HiZMaxOut = graph.Write(ctx.GetHiZResource(HiZReductionMode::Max), Compute | Storage);

                passData.MinSampler = ctx.GetMinMaxSampler(HiZReductionMode::Min);
                passData.MaxSampler = ctx.GetMinMaxSampler(HiZReductionMode::Max);
                passData.MipmapViews = ctx.GetViewHandles();

                passData.MinMaxDepth = graph.GetBlackboard().Get<HiZBlit::PassData>(minBlit).MinMaxDepth;
                
                ctx.SetHiZResource(passData.HiZMinOut, HiZReductionMode::Min);
                ctx.SetHiZResource(passData.HiZMaxOut, HiZReductionMode::Max);
                
                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZFull")
                GPU_PROFILE_FRAME("HiZFull")

                Texture hizMinOutput = resources.GetTexture(passData.HiZMinOut);
                Texture hizMaxOutput = resources.GetTexture(passData.HiZMaxOut);

                const Shader& shader = resources.GetGraph()->GetShader();
                DepthReductionShaderBindGroup bindGroup(shader);

                bindGroup.SetMinSampler(passData.MinSampler);
                bindGroup.SetMaxSampler(passData.MaxSampler);
                bindGroup.SetMinImage({.Image = hizMinOutput}, ImageLayout::General);
                bindGroup.SetMaxImage({.Image = hizMaxOutput}, ImageLayout::General);
                
                for (u32 i = 0; i < passData.MipmapViews.size(); i++)
                {
                    bindGroup.SetOutputMin({
                        .Image = hizMinOutput,
                        .Description = passData.MipmapViews[i]}, ImageLayout::General, i);
                    bindGroup.SetOutputMax({
                        .Image = hizMaxOutput,
                        .Description = passData.MipmapViews[i]}, ImageLayout::General, i);
                }

                u32 pushConstant = currentMipmap << MIPMAP_LEVEL_SHIFT | toBeProcessed;
                auto& cmd = frameContext.Cmd;
                bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
                RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstant);
                u32 shift = toBeProcessed > 5 ? 12 : 10;
                u32 mask = toBeProcessed > 5 ? 4095 : 1023;
                u32 samples = width * height;
                RenderCommand::Dispatch(cmd, {(samples + mask) >> shift, 1, 1});
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
