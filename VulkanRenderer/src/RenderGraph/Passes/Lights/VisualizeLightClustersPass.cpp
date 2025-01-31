#include "VisualizeLightClustersPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/LightClustersVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::LightClustersVisualize::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
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

            passData.ColorOut = graph.CreateResource(std::string{name} + ".Color",
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

            bindGroup.SetDepth(resources.GetTexture(depth).BindingInfo(ImageFilter::Linear, ImageLayout::Readonly));
            bindGroup.SetClusters({.Buffer = resources.GetBuffer(passData.Clusters)});
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});

            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            RenderCommand::Draw(cmd, 3);
        });
}
