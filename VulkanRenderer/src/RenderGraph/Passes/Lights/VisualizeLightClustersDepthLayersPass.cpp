#include "VisualizeLightClustersDepthLayersPass.h"

#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
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
            
            auto& depthDescription = Resources(graph).GetTextureDescription(depth);
            passData.ColorOut = graph.CreateResource("Color"_hsv,
                GraphTextureDescription{
                    .Width = depthDescription.Width,
                    .Height = depthDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.Depth = graph.Read(depth, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Depth")
            GPU_PROFILE_FRAME("Lights.Clusters.Visualize.Depth")

            Texture depthTexture = resources.GetTexture(passData.Depth);

            const Shader& shader = resources.GetGraph()->GetShader();
            LightClustersDepthLayersVisualizeShaderBindGroup bindGroup(shader);
            bindGroup.SetDepth({.Image = depthTexture}, ImageLayout::Readonly);

            struct PushConstant
            {
                f32 Near;
                f32 Far;
            };
            PushConstant pushConstant = {
                .Near = frameContext.PrimaryCamera->GetNear(),
                .Far = frameContext.PrimaryCamera->GetFar()};

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstant}});
            cmd.Draw({.VertexCount = 3});
        }).Data;
}
