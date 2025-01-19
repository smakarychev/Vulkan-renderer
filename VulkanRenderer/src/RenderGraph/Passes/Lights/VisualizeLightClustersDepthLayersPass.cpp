#include "VisualizeLightClustersDepthLayersPass.h"

#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::LightClustersDepthLayersVisualize::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Depth.Setup")

            graph.SetShader("light-clusters-depth-layers-visualize.shader");
            
            auto& depthDescription = Resources(graph).GetTextureDescription(depth);
            passData.ColorOut = graph.CreateResource(std::string{name} + ".Color",
                GraphTextureDescription{
                    .Width = depthDescription.Width,
                    .Height = depthDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.Depth = graph.Read(depth, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Visualize.Depth")
            GPU_PROFILE_FRAME("Lights.Clusters.Visualize.Depth")

            const Texture& depthTexture = resources.GetTexture(passData.Depth);

            const Shader& shader = resources.GetGraph()->GetShader();
            auto pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_depth", depthTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));

            struct PushConstant
            {
                f32 Near;
                f32 Far;
            };
            PushConstant pushConstant = {
                .Near = frameContext.PrimaryCamera->GetNear(),
                .Far = frameContext.PrimaryCamera->GetFar()};

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, shader.GetLayout());
            RenderCommand::BindGraphics(cmd, pipeline);
            RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstant);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());

            RenderCommand::Draw(cmd, 3);
        });
}
