#pragma once

#include "HiZCommon.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/HizBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace Passes::HiZBlit
{
    struct ExecutionInfo
    {
        RG::Resource Depth{};
        HiZ::ReductionMode ReductionMode{HiZ::ReductionMode::Min};
        bool CalculateMinMax{false};
    };
    struct PassData
    {
        RG::Resource MinMaxDepth{};
        RG::Resource Depth{};
        RG::Resource HiZ{};
        std::array<RG::Resource, HiZ::MAX_MIP_LEVELS> HiZMips{};
    };
    inline PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
    {
        using namespace RG;
        using enum ResourceAccessFlags;

        return renderGraph.AddRenderPass<PassData>(name.Concatenate(".Blit"),
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("HiZ.Blit.Setup")

                graph.SetShader("hiz"_hsv,
                    ShaderSpecializations{
                        ShaderSpecialization{"DEPTH_MIN_MAX"_hsv, info.CalculateMinMax}});

                Resource depth = info.Depth;
                Resource hiz = HiZ::createHiz(graph, graph.GetImageDescription(depth).Dimensions());

                passData.HiZ = hiz;
                passData.HiZMips[0] = graph.SplitImage(hiz, {.MipmapBase = 0, .Mipmaps = 1});

                if (info.CalculateMinMax)
                {
                    passData.MinMaxDepth = HiZ::createMinMaxBufferResource(graph);
                    passData.MinMaxDepth = graph.ReadWriteBuffer(passData.MinMaxDepth, Compute | Storage);
                    passData.MinMaxDepth = graph.Upload(passData.MinMaxDepth, HiZ::MinMaxDepth{});
                }
                
                passData.Depth = graph.ReadImage(depth, Compute | Sampled);
                passData.HiZMips[0] = graph.WriteImage(passData.HiZMips[0], Compute | Storage);
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("HiZ.Blit")
                GPU_PROFILE_FRAME("HiZ.Blit")
                
                const glm::uvec2 hizResolution = graph.GetImageDescription(passData.HiZ).Dimensions();
                
                const Shader& shader = graph.GetShader();
                HizShaderBindGroup bindGroup(shader);
                bindGroup.SetInSampler(HiZ::createSampler(info.ReductionMode));
                bindGroup.SetInImage(graph.GetImageBinding(passData.Depth));
                bindGroup.SetOutImage(graph.GetImageBinding(passData.HiZMips[0]));
                if (info.CalculateMinMax)
                    bindGroup.SetMinMax(graph.GetBufferBinding(passData.MinMaxDepth));

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {hizResolution}});
                cmd.Dispatch({
                    .Invocations = {(hizResolution.x + 32 - 1) / 32, (hizResolution.y + 32 - 1) / 32, 1}});
            });
    }
}
