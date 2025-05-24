#include "VisualizeLightClustersDepthLayersPass.h"

#include "Core/Camera.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/LightClustersDepthLayersVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::LightClustersDepthLayersVisualize::PassData& Passes::LightClustersDepthLayersVisualize::addToGraph(
    StringId name, RG::Graph& renderGraph, RG::Resource depth)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Depth.Setup")

            graph.SetShader("light-clusters-depth-layers-visualize"_hsv);
            
            passData.ColorOut = graph.Create("Color"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Size,
                    .Reference = depth,
                    .Format = Format::RGBA16_FLOAT});

            passData.Depth = graph.ReadImage(depth, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {});
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Depth")
            GPU_PROFILE_FRAME("Lights.Clusters.Visualize.Depth")

            const Shader& shader = graph.GetShader();
            LightClustersDepthLayersVisualizeShaderBindGroup bindGroup(shader);
            bindGroup.SetDepth(graph.GetImageBinding(passData.Depth));

            struct PushConstant
            {
                f32 Near;
                f32 Far;
            };
            PushConstant pushConstant = {
                .Near = frameContext.PrimaryCamera->GetNear(),
                .Far = frameContext.PrimaryCamera->GetFar()};

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstant}});
            cmd.Draw({.VertexCount = 3});
        });
}
