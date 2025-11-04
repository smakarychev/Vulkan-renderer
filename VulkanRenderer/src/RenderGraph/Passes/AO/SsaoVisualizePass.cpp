#include "rendererpch.h"

#include "SsaoVisualizePass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SsaoVisualizeBindGroupRG.generated.h"

Passes::SsaoVisualize::PassData& Passes::SsaoVisualize::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource ssao)
{
    using namespace RG;

    struct PassDataPrivate : PassDataWithBind<PassData, SsaoVisualizeBindGroupRG>
    {
        Resource SSAO{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Visualize.Setup")

            passData.BindGroup = SsaoVisualizeBindGroupRG(graph, graph.SetShader("ssaoVisualize"_hsv));
            
            passData.Color = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Size,
                    .Reference = ssao,
                    .Format = SsaoVisualizeBindGroupRG::GetSsaoAttachmentFormat()});

            passData.SSAO = passData.BindGroup.SetResourcesSsao(ssao);
            passData.Color = graph.RenderTarget(passData.Color, {});
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("SSAO.Visualize")
            GPU_PROFILE_FRAME("SSAO.Visualize")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd, graph.GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
