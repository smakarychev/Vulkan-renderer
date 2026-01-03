#include "rendererpch.h"

#include "VisualizeLightClustersCascadesPass.h"

#include "Core/Camera.h"
#include "RenderGraph/Passes/Generated/LightClustersVisualizeCascadesBindGroupRG.generated.h"

Passes::LightClustersCascadesVisualize::PassData& Passes::LightClustersCascadesVisualize::addToGraph(
    StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, LightClustersVisualizeCascadesBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Cascades.Setup")

            passData.BindGroup = LightClustersVisualizeCascadesBindGroupRG(graph);
            
            passData.ColorOut = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Size,
                    .Reference = info.Depth,
                    .Format = Format::RGBA16_FLOAT});

            passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.BindGroup.SetResourcesDepth(info.Depth);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {});
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Cascades")
            GPU_PROFILE_FRAME("Lights.Clusters.Visualize.Cascades")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
