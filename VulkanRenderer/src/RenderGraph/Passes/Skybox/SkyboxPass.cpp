#include "SkyboxPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Skybox::addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& skybox,
    RG::Resource colorOut, RG::Resource depthIn, const glm::uvec2& resolution, f32 lodBias)
{
    return addToGraph(name, renderGraph, renderGraph.AddExternal(std::string{name} + ".Skybox", skybox),
        colorOut, depthIn, resolution, lodBias);
}

RG::Pass& Passes::Skybox::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource skybox,
    RG::Resource colorOut, RG::Resource depthIn, const glm::uvec2& resolution, f32 lodBias)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Skybox.Setup")

            graph.SetShader("../assets/shaders/skybox.shader");
            
            passData.ColorOut = RgUtils::ensureResource(colorOut, graph, std::string{name} + ".Color",
                GraphTextureDescription{
                    .Width = resolution.x,
                    .Height = resolution.y,
                    .Format = Format::RGBA16_FLOAT});
            ASSERT(depthIn.IsValid(), "Depth has to be provided")

            passData.Projection = graph.CreateResource(std::string{name} + ".Projection", GraphBufferDescription{
                .SizeBytes = sizeof(ProjectionUBO)});
            
            auto& globalResources = graph.GetGlobalResources();
      
            passData.Skybox = graph.Read(skybox, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);
            passData.DepthOut = graph.DepthStencilTarget(depthIn, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Projection = graph.Read(passData.Projection, Vertex | Uniform);
            ProjectionUBO projection = {
                .ProjectionInverse = glm::inverse(globalResources.PrimaryCamera->GetProjection()),
                .ViewInverse = glm::inverse(globalResources.PrimaryCamera->GetView())};
            graph.Upload(passData.Projection, projection);

            passData.ShadingSettings = graph.Read(globalResources.ShadingSettings, Pixel | Uniform);

            passData.LodBias = lodBias;

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Skybox")
            GPU_PROFILE_FRAME("Skybox")

            const Texture& skyboxTexture = resources.GetTexture(passData.Skybox);
            const Buffer projectionBuffer = resources.GetBuffer(passData.Projection);
            
            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_skybox", skyboxTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_projection", projectionBuffer.BindingInfo());
            resourceDescriptors.UpdateBinding("u_shading", resources.GetBuffer(passData.ShadingSettings).BindingInfo());
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, shader.GetLayout());
            pipeline.BindGraphics(cmd);
            RenderCommand::PushConstants(cmd, shader.GetLayout(), passData.LodBias);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            RenderCommand::Draw(cmd, 6);
        });

    return pass;
}
