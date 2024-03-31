#include "SkyboxPass.h"

#include "FrameContext.h"
#include "Core/Camera.h"
#include "Vulkan/RenderCommand.h"

SkyboxPass::SkyboxPass(RenderGraph::Graph& renderGraph)
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

void SkyboxPass::AddToGraph(RenderGraph::Graph& renderGraph, const Texture& skybox, RenderGraph::Resource colorOut,
    RenderGraph::Resource depthIn, const glm::uvec2& resolution)
{
    using namespace RenderGraph;
    std::string name = "Skybox";
    AddToGraph(renderGraph, renderGraph.AddExternal(name + ".Skybox", skybox), colorOut, depthIn, resolution);
}

void SkyboxPass::AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource skybox,
    RenderGraph::Resource colorOut, RenderGraph::Resource depthIn, const glm::uvec2& resolution)
{
    using namespace RenderGraph;
    using enum ResourceAccessFlags;
    
    std::string name = "Skybox";
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            passData.ColorOut = colorOut;
            if (!passData.ColorOut.IsValid())
            {
                passData.ColorOut = graph.CreateResource(name + ".Color", GraphTextureDescription{
                    .Width = resolution.x,
                    .Height = resolution.y,
                    .Format = Format::RGBA16_FLOAT});
            }
            ASSERT(depthIn.IsValid(), "Depth has to be provided")
      
            passData.Skybox = graph.Read(skybox, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Unspecified, AttachmentStore::Store);
            passData.DepthIn = graph.DepthStencilTarget(depthIn, AttachmentLoad::Load, AttachmentStore::Store);

            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().UpdateOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Skybox")

            const Texture& skyboxTexture = resources.GetTexture(passData.Skybox);
            
            struct PushConstants
            {
                glm::mat4 ProjectionInverse{};
                glm::mat4 ViewInverse{};
            };

            PushConstants pushConstants = {
                .ProjectionInverse = glm::inverse(frameContext.MainCamera->GetProjection()),
                .ViewInverse = glm::inverse(frameContext.MainCamera->GetView())};

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding("u_skybox", skyboxTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Draw(cmd, 6);
        });
}
