#include "rendererpch.h"

#include "VisualizeLightClustersPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/LightClustersVisualizeBindGroupRG.generated.h"

Passes::LightClustersVisualize::PassData& Passes::LightClustersVisualize::addToGraph(
    StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    using PassDataBind = PassDataWithBind<PassData, LightClustersVisualizeBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Setup")

            passData.BindGroup = LightClustersVisualizeBindGroupRG(graph);

            passData.Color = graph.Create("Color"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d,
                .Reference = info.Depth,
                .Format = LightClustersVisualizeBindGroupRG::GetClustersAttachmentFormat()
            });

            passData.BindGroup.SetResourcesDepth(info.Depth);
            passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.BindGroup.SetResourcesClusters(info.Clusters);
            
            passData.Color = graph.RenderTarget(passData.Color, {});
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize")
            GPU_PROFILE_FRAME("Lights.Clusters.Visualize")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
