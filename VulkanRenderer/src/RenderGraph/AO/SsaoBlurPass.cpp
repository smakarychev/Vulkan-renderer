#include "SsaoBlurPass.h"

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
