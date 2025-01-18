#include "VisualizeLightClustersPass.h"

#include "RenderGraph/RenderGraph.h"
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

            graph.SetShader("../assets/shaders/light-clusters-visualize.shader");

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
            auto pipeline = shader.Pipeline();
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_depth", resources.GetTexture(depth).BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));

            resourceDescriptors.UpdateBinding("u_clusters", resources.GetBuffer(passData.Clusters).BindingInfo());
            resourceDescriptors.UpdateBinding("u_camera", resources.GetBuffer(passData.Camera).BindingInfo());

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, shader.GetLayout());
            RenderCommand::BindGraphics(cmd, pipeline);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            RenderCommand::Draw(cmd, 3);
        });
}
