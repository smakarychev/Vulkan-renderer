#include "rendererpch.h"

#include "VisualizeLightTiles.h"

#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Generated/LightTilesVisualizeBindGroupRG.generated.h"

Passes::LightTilesVisualize::PassData& Passes::LightTilesVisualize::addToGraph(
    StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, LightTilesVisualizeBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Visualize.Setup")

            passData.BindGroup = LightTilesVisualizeBindGroupRG(graph);

            auto& globalResources = graph.GetGlobalResources();

            passData.Color = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Width = (f32)globalResources.Resolution.x,
                    .Height = (f32)globalResources.Resolution.y,
                    .Format = LightTilesVisualizeBindGroupRG::GetTilesAttachmentFormat()
                });

            if (info.Bins.IsValid())
                passData.BindGroup.SetResourcesZbins(info.Bins);
            passData.BindGroup.SetResourcesDepth(info.Depth);
            passData.BindGroup.SetResourcesTiles(info.Tiles);
            passData.BindGroup.SetResourcesView(info.ViewInfo);

            passData.Color = graph.RenderTarget(passData.Color, {});
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Visualize")
            GPU_PROFILE_FRAME("Lights.Tiles.Visualize")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.Draw({.VertexCount = 3});
        });
}
