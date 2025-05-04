#include "VisualizeLightClustersPass.h"

#include "RenderGraph/RenderGraph.h"
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

            passData.Color = graph.CreateResource("Color"_hsv,
                GraphTextureDescription{
                    .Width = globalResources.Resolution.x,
                    .Height = globalResources.Resolution.y,
                    .Format = Format::RGBA16_FLOAT});
            
            passData.Color = graph.RenderTarget(passData.Color, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Depth = graph.Read(info.Depth, Pixel | Sampled);
            
            passData.Clusters = graph.Read(info.Clusters, Pixel | Storage);
            passData.Camera = graph.Read(globalResources.PrimaryCameraGPU, Pixel | Uniform);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize")
            GPU_PROFILE_FRAME("Lights.Clusters.Visualize")

            const Shader& shader = resources.GetGraph()->GetShader();
            LightClustersVisualizeShaderBindGroup bindGroup(shader);

            bindGroup.SetDepth({.Image = resources.GetTexture(passData.Depth)}, ImageLayout::Readonly);
            bindGroup.SetClusters({.Buffer = resources.GetBuffer(passData.Clusters)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        }).Data;
}
