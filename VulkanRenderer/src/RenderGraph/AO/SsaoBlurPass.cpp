#include "SsaoBlurPass.h"

#include "FrameContext.h"
#include "Vulkan/RenderCommand.h"

SsaoBlurPass::SsaoBlurPass(RenderGraph::Graph& renderGraph, SsaoBlurPassKind kind)
    : m_Name(std::string("SSAO.Blur") + (kind == SsaoBlurPassKind::Horizontal ? "Horizontal" : "Vertical"))
{
    ShaderPipelineTemplate* ssaoTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/common/fullscreen-vert.shader",
        "../assets/shaders/processed/render-graph/ao/ssao-blur-frag.shader"},
        "Pass.SSAO.Blur", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(ssaoTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::R8_UNORM}})
        .UseDescriptorBuffer()
        .AddSpecialization("IS_VERTICAL", kind == SsaoBlurPassKind::Vertical)
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

void SsaoBlurPass::AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource ssao, RenderGraph::Resource colorOut)
{
    using namespace RenderGraph;
    using enum ResourceAccessFlags;

    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            passData.SsaoOut = colorOut;
            if (!passData.SsaoOut.IsValid())
            {
                const TextureDescription& ssaoDescription = Resources(graph).GetTextureDescription(ssao);
                passData.SsaoOut = graph.CreateResource(m_Name.Name() + ".ColorOut", GraphTextureDescription{
                    .Width = ssaoDescription.Width,
                    .Height = ssaoDescription.Height,
                    .Format = Format::R8_UNORM});
            }

            passData.SsaoIn = graph.Read(ssao, Pixel | Sampled);
            passData.SsaoOut = graph.RenderTarget(passData.SsaoOut, AttachmentLoad::Load, AttachmentStore::Store);

            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().RegisterOutput(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            const Texture& ssaoIn = resources.GetTexture(passData.SsaoIn);

            auto& pipeline = passData.PipelineData->Pipeline;    
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;    
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding("u_ssao", ssaoIn.CreateBindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            
            RenderCommand::Draw(cmd, 3);
        });
}
