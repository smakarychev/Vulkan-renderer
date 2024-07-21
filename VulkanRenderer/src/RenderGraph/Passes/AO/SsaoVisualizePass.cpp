#include "SsaoVisualizePass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "Vulkan/RenderCommand.h"

SsaoVisualizePass::SsaoVisualizePass(RG::Graph& renderGraph)
{
    ShaderPipelineTemplate* ssaoTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
       "../assets/shaders/processed/render-graph/common/fullscreen-vert.stage",
       "../assets/shaders/processed/render-graph/ao/ssao-visualize-frag.stage"},
       "Pass.SSAO.Visualize", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(ssaoTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT}})
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(ssaoTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(ssaoTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void SsaoVisualizePass::AddToGraph(RG::Graph& renderGraph, RG::Resource ssao,
    RG::Resource colorOut)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    std::string name = "SSAO.Visualize";
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            auto& ssaoDescription = Resources(graph).GetTextureDescription(ssao);
            passData.ColorOut = RgUtils::ensureResource(colorOut, graph, name + ".Color",
                GraphTextureDescription{
                    .Width = ssaoDescription.Width,
                    .Height = ssaoDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.SSAO = graph.Read(ssao, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);

            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().Update(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Visualize SSAO")

            const Texture& ssaoTexture = resources.GetTexture(passData.SSAO);

            auto& pipeline = passData.PipelineData->Pipeline;    
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;    
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding("u_ssao", ssaoTexture.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));

            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            RenderCommand::Draw(cmd, 3);
        });
}
