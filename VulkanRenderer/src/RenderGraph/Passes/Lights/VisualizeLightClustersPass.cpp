#include "VisualizeLightClustersPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Generated/LightClustersVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::LightClustersVisualize::PassData& Passes::LightClustersVisualize::addToGraph(
    StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    struct PassDataPrivate : PassData
    {
        Resource Clusters{};
        Resource Camera{};
        Resource Depth{};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Setup")

            graph.SetShader("light-clusters-depth-layers-visualize"_hsv);

            auto& globalResources = graph.GetGlobalResources();

            passData.Color = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Width = (f32)globalResources.Resolution.x,
                    .Height = (f32)globalResources.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});
            
            passData.Color = graph.RenderTarget(passData.Color, {});
            passData.Depth = graph.ReadImage(info.Depth, Pixel | Sampled);
            
            passData.Clusters = graph.ReadBuffer(info.Clusters, Pixel | Storage);
            passData.Camera = graph.ReadBuffer(globalResources.PrimaryCameraGPU, Pixel | Uniform);
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize")
            GPU_PROFILE_FRAME("Lights.Clusters.Visualize")

            const Shader& shader = graph.GetShader();
            LightClustersVisualizeShaderBindGroup bindGroup(shader);

            bindGroup.SetDepth(graph.GetImageBinding(passData.Depth));
            bindGroup.SetClusters(graph.GetBufferBinding(passData.Clusters));
            bindGroup.SetCamera(graph.GetBufferBinding(passData.Camera));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
