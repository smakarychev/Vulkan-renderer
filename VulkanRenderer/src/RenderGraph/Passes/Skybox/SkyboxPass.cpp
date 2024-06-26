#include "SkyboxPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "RenderGraph/RGUtils.h"
#include "Vulkan/RenderCommand.h"

SkyboxPass::SkyboxPass(RG::Graph& renderGraph)
{
    ShaderPipelineTemplate* skyboxTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/general/skybox-vert.shader",
          "../assets/shaders/processed/render-graph/general/skybox-frag.shader"},
      "Pass.Skybox", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(skyboxTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT},
            .DepthFormat = Format::D32_FLOAT})
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(skyboxTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(skyboxTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void SkyboxPass::AddToGraph(RG::Graph& renderGraph, const Texture& skybox, RG::Resource colorOut,
    RG::Resource depthIn, const glm::uvec2& resolution, f32 lodBias)
{
    using namespace RG;
    std::string name = "Skybox";
    AddToGraph(renderGraph, renderGraph.AddExternal(name + ".Skybox", skybox), colorOut, depthIn, resolution, lodBias);
}

void SkyboxPass::AddToGraph(RG::Graph& renderGraph, RG::Resource skybox,
    RG::Resource colorOut, RG::Resource depthIn, const glm::uvec2& resolution, f32 lodBias)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    std::string name = "Skybox";
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            passData.ColorOut = RgUtils::ensureResource(colorOut, graph, name + ".Color",
                GraphTextureDescription{
                    .Width = resolution.x,
                    .Height = resolution.y,
                    .Format = Format::RGBA16_FLOAT});
            ASSERT(depthIn.IsValid(), "Depth has to be provided")

            passData.Projection = graph.CreateResource(name + ".Projection", GraphBufferDescription{
                .SizeBytes = sizeof(ProjectionUBO)});
      
            passData.Skybox = graph.Read(skybox, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Load, AttachmentStore::Store);
            passData.DepthOut = graph.DepthStencilTarget(depthIn, AttachmentLoad::Load, AttachmentStore::Store);
            passData.Projection = graph.Read(passData.Projection, Vertex | Uniform | Upload);

            passData.PipelineData = &m_PipelineData;
            passData.LodBias = lodBias;

            graph.GetBlackboard().Update(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Skybox")

            const Texture& skyboxTexture = resources.GetTexture(passData.Skybox);

            ProjectionUBO projection = {
                .ProjectionInverse = glm::inverse(frameContext.MainCamera->GetProjection()),
                .ViewInverse = glm::inverse(frameContext.MainCamera->GetView())};
            const Buffer projectionBuffer = resources.GetBuffer(passData.Projection, projection,
                *frameContext.ResourceUploader);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding("u_skybox", skyboxTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_projection", projectionBuffer.BindingInfo());
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), passData.LodBias);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Draw(cmd, 6);
        });
}
