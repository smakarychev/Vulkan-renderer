#include "VisualizeLightClustersPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/LightClustersVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::LightClustersVisualize::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource depth,
    RG::Resource clusters)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Setup")

            graph.SetShader("light-clusters-visualize.shader");

            auto& globalResources = graph.GetGlobalResources();

            passData.ColorOut = graph.CreateResource("Color"_hsv,
                GraphTextureDescription{
                    .Width = globalResources.Resolution.x,
                    .Height = globalResources.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});
            
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Depth = graph.Read(depth, Pixel | Sampled);
            
            passData.Clusters = graph.Read(clusters, Pixel | Storage);
            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Pixel | Uniform);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize")
            GPU_PROFILE_FRAME("Lights.Clusters.Visualize")

            const Shader& shader = resources.GetGraph()->GetShader();
            LightClustersVisualizeShaderBindGroup bindGroup(shader);

            bindGroup.SetDepth({.Image = resources.GetTexture(depth)}, ImageLayout::Readonly);
            bindGroup.SetClusters({.Buffer = resources.GetBuffer(passData.Clusters)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
