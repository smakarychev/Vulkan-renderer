#pragma once
#include "HiZPassContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/Passes/Generated/HizBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace Passes::HiZBlit
{
    struct MinMaxDepth
    {
        u32 Min{std::bit_cast<u32>(1.0f)};
        u32 Max{std::bit_cast<u32>(0.0f)};
    };
    struct PassData
    {
        Sampler MinMaxSampler;
        Span<const ImageSubresourceDescription> MipmapViews;
        
        RG::Resource MinMaxDepth{};
        RG::Resource DepthIn{};
        RG::Resource HiZOut{};
    };
    inline RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
        ImageSubresourceDescription subresource, HiZPassContext& ctx, HiZReductionMode mode,
        bool minMaxDepth = false)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        const TextureDescription& hizDescription = Device::GetImageDescription(ctx.GetHiZ(mode));
        u32 width = hizDescription.Width;  
        u32 height = hizDescription.Height;

        return renderGraph.AddRenderPass<PassData>(PassName{std::format("{}.Blit", name)},
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZ.Blit.Setup")

                graph.SetShader("hiz.shader",
                    ShaderOverrides{
                        ShaderOverride{"DEPTH_MIN_MAX"_hsv, minMaxDepth}});
                
                Resource depthIn = depth;
                Resource depthOut = graph.AddExternal("Hiz.Out", ctx.GetHiZ(mode));
                graph.Export(depthOut, ctx.GetHiZPrevious(mode), true);

                if (minMaxDepth)
                {
                    passData.MinMaxDepth = graph.AddExternal(std::format("{}.MinMaxDepth", name),
                        ctx.GetMinMaxDepthBuffer());
                    passData.MinMaxDepth = graph.Read(passData.MinMaxDepth, Compute | Storage);
                    passData.MinMaxDepth = graph.Write(passData.MinMaxDepth, Compute | Storage);

                    graph.Upload(passData.MinMaxDepth, MinMaxDepth{});
                }
                
                passData.DepthIn = graph.Read(depthIn, Compute | Sampled);
                passData.HiZOut = graph.Write(depthOut, Compute | Storage);

                passData.MinMaxSampler = ctx.GetMinMaxSampler(mode);
                passData.MipmapViews = ctx.GetViewHandles();

                ctx.SetHiZResource(passData.HiZOut, mode);
                
                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZ.Blit")
                GPU_PROFILE_FRAME("HiZ.Blit")
                
                Texture depthIn = resources.GetTexture(passData.DepthIn);
                Texture hizOut = resources.GetTexture(passData.HiZOut);

                const Shader& shader = resources.GetGraph()->GetShader();
                HizShaderBindGroup bindGroup(shader);
                bindGroup.SetInSampler(passData.MinMaxSampler);
                bindGroup.SetInImage({.Image = depthIn, .Description = subresource}, ImageLayout::DepthReadonly);
                bindGroup.SetOutImage({.Image = hizOut,  .Description = passData.MipmapViews[0]}, ImageLayout::General);
                if (minMaxDepth)
                    bindGroup.SetMinMax({.Buffer = resources.GetBuffer(passData.MinMaxDepth)});
                
                glm::uvec2 levels = {width, height};
                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
                frameContext.CommandList.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {levels}});
                frameContext.CommandList.Dispatch({
                    .Invocations = {(width + 32 - 1) / 32, (height + 32 - 1) / 32, 1}});
            });
    }
}
