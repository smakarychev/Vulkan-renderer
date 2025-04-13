#pragma once

#include "HiZCommon.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/HizBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace Passes::HiZBlit
{
    struct ExecutionInfo
    {
        RG::Resource Depth{};
        ImageSubresourceDescription Subresource{};
        HiZ::ReductionMode ReductionMode{HiZ::ReductionMode::Min};
        bool CalculateMinMax{false};
    };
    struct PassData
    {
        RG::Resource MinMaxDepth{};
        RG::Resource Depth{};
        RG::Resource HiZ{};
    };
    inline RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        return renderGraph.AddRenderPass<PassData>(name.Concatenate(".Blit"),
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZ.Blit.Setup")

                graph.SetShader("hiz.shader",
                    ShaderOverrides{
                        ShaderOverride{"DEPTH_MIN_MAX"_hsv, info.CalculateMinMax}});

                Resource depth = info.Depth;
                Resource hiz = HiZ::createHiz(graph, graph.GetTextureDescription(depth).Dimensions());

                if (info.CalculateMinMax)
                {
                    passData.MinMaxDepth = HiZ::createMinMaxBuffer(graph);
                    passData.MinMaxDepth = graph.Read(passData.MinMaxDepth, Compute | Storage);
                    passData.MinMaxDepth = graph.Write(passData.MinMaxDepth, Compute | Storage);

                    graph.Upload(passData.MinMaxDepth, HiZ::MinMaxDepth{});
                }
                
                passData.Depth = graph.Read(depth, Compute | Sampled);
                passData.HiZ = graph.Write(hiz, Compute | Storage);

                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("HiZ.Blit")
                GPU_PROFILE_FRAME("HiZ.Blit")
                
                Texture depth = resources.GetTexture(passData.Depth);
                Texture hiz = resources.GetTexture(passData.HiZ);
                glm::uvec2 hizResolution = Device::GetImageDescription(hiz).Dimensions();
                Span hizViews = Device::GetAdditionalImageViews(hiz);
                
                const Shader& shader = resources.GetGraph()->GetShader();
                HizShaderBindGroup bindGroup(shader);
                bindGroup.SetInSampler(HiZ::createSampler(info.ReductionMode));
                bindGroup.SetInImage({.Image = depth, .Description = info.Subresource},
                    ImageLayout::DepthReadonly);
                bindGroup.SetOutImage({.Image = hiz,  .Description = hizViews[0]}, ImageLayout::General);
                if (info.CalculateMinMax)
                    bindGroup.SetMinMax({.Buffer = resources.GetBuffer(passData.MinMaxDepth)});

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {hizResolution}});
                cmd.Dispatch({
                    .Invocations = {(hizResolution.x + 32 - 1) / 32, (hizResolution.y + 32 - 1) / 32, 1}});
            });
    }
}
