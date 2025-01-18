#include "VisualizeDepthPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::VisualizeDepth::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depthIn,
    RG::Resource colorIn, f32 near, f32 far, bool isOrthographic)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            graph.SetShader("../assets/shaders/depth-visualize.shader");
            
            auto& depthDescription = Resources(graph).GetTextureDescription(depthIn);
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, std::string{name} + ".Color",
                GraphTextureDescription{
                    .Width = depthDescription.Width,
                    .Height = depthDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.DepthIn = graph.Read(depthIn, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Visualize Depth")

            const Texture& depthTexture = resources.GetTexture(passData.DepthIn);

            const Shader& shader = resources.GetGraph()->GetShader();
            auto pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_depth", depthTexture.BindingInfo(
                ImageFilter::Linear, depthTexture.Description().Format == Format::D32_FLOAT ?
                ImageLayout::DepthReadonly :
                ImageLayout::DepthStencilReadonly));

            struct PushConstants
            {
                f32 Near{1.0f};
                f32 Far{100.0f};
                bool IsOrthographic{false};
            };
            PushConstants pushConstants = {
                .Near = near,
                .Far = far,
                .IsOrthographic = isOrthographic};
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, shader.GetLayout());
            RenderCommand::BindGraphics(cmd, pipeline);
            RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstants);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            
            RenderCommand::Draw(cmd, 3);
        });

    return pass;
}
