#include "rendererpch.h"

#include "VisualizeLightTiles.h"

#include "Light/LightZBinner.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Generated/LightTilesVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

Passes::LightTilesVisualize::PassData& Passes::LightTilesVisualize::addToGraph(
    StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate : PassData
    {
        Resource Tiles{};
        Resource ViewInfo{};
        Resource ZBins{};
        Resource Depth{};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Visualize.Setup")

            graph.SetShader("light-tiles-visualize"_hsv);

            auto& globalResources = graph.GetGlobalResources();

            passData.Color = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Width = (f32)globalResources.Resolution.x,
                    .Height = (f32)globalResources.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            passData.ZBins = {};
            if (info.Bins.IsValid())
                passData.ZBins = graph.ReadBuffer(info.Bins, Pixel | Storage);
            
            passData.Color = graph.RenderTarget(passData.Color, {});
            passData.Depth = graph.ReadImage(info.Depth, Pixel | Sampled);
            passData.Tiles = graph.ReadBuffer(info.Tiles, Pixel | Storage);

            passData.ViewInfo = graph.ReadBuffer(globalResources.PrimaryViewInfoResource, Pixel | Uniform);
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Visualize")
            GPU_PROFILE_FRAME("Lights.Tiles.Visualize")

            const Shader& shader = graph.GetShader();
            LightTilesVisualizeShaderBindGroup bindGroup(shader);
            bindGroup.SetDepth(graph.GetImageBinding(passData.Depth));
            bindGroup.SetTiles(graph.GetBufferBinding(passData.Tiles));
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));

            bool useZBins = passData.ZBins.IsValid();
            if (useZBins)
                bindGroup.SetZbins(graph.GetBufferBinding(passData.ZBins));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {useZBins}});
            cmd.Draw({.VertexCount = 3});
        });
}
