#pragma once
#include "HiZPassContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/Passes/Generated/HizBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

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
        std::vector<ImageViewHandle> MipmapViewHandles;
        
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

        u32 width = ctx.GetHiZ(mode).Description().Width;  
        u32 height = ctx.GetHiZ(mode).Description().Height;

        return renderGraph.AddRenderPass<PassData>(PassName{std::format("{}.Blit", name)},
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZ.Blit.Setup")

                graph.SetShader("hiz.shader",
                    ShaderOverrides{
                        ShaderOverride{{"DEPTH_MIN_MAX"}, minMaxDepth}});
                
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
                passData.MipmapViewHandles = ctx.GetViewHandles();

                ctx.SetHiZResource(passData.HiZOut, mode);
                
                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZ.Blit")
                GPU_PROFILE_FRAME("HiZ.Blit")
                
                const Texture& depthIn = resources.GetTexture(passData.DepthIn);
                const Texture& hizOut = resources.GetTexture(passData.HiZOut);
                
                TextureBindingInfo depthInBinding = depthIn.BindingInfo(passData.MinMaxSampler,
                    ImageLayout::DepthReadonly, depthIn.GetViewHandle(subresource));

                const Shader& shader = resources.GetGraph()->GetShader();
                HizShaderBindGroup bindGroup(shader);
                bindGroup.SetInSampler(depthInBinding);
                bindGroup.SetInImage(depthInBinding);
                bindGroup.SetOutImage(hizOut.BindingInfo(
                        passData.MinMaxSampler, ImageLayout::General, passData.MipmapViewHandles[0]));
                if (minMaxDepth)
                    bindGroup.SetMinMax({.Buffer = resources.GetBuffer(passData.MinMaxDepth)});
                
                glm::uvec2 levels = {width, height};
                auto& cmd = frameContext.Cmd;
                bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
                RenderCommand::PushConstants(cmd, shader.GetLayout(), levels);
                RenderCommand::Dispatch(cmd, {(width + 32 - 1) / 32, (height + 32 - 1) / 32, 1});
            });
    }
}
