#include "SsaoBlurPass.h"

#include "FrameContext.h"
#include "Vulkan/RenderCommand.h"

SsaoBlurPass::SsaoBlurPass(RenderGraph::Graph& renderGraph, SsaoBlurPassKind kind)
    : m_Name(std::string("SSAO.Blur") + (kind == SsaoBlurPassKind::Horizontal ? "Horizontal" : "Vertical"))
{
    ShaderPipelineTemplate* ssaoTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/ao/ssao-blur-comp.shader"},
        "Pass.SSAO.Blur", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(ssaoTemplate)
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

            passData.SsaoIn = graph.Read(ssao, Compute | Sampled);
            passData.SsaoOut = graph.Write(passData.SsaoOut, Compute | Storage);

            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().RegisterOutput(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("SSAO.Blur")
            
            const Texture& ssaoIn = resources.GetTexture(passData.SsaoIn);
            const Texture& ssaoOut = resources.GetTexture(passData.SsaoOut);

            auto& pipeline = passData.PipelineData->Pipeline;    
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;    
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding("u_ssao", ssaoIn.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_ssao_blurred", ssaoOut.BindingInfo(
                ImageFilter::Linear, ImageLayout::General));
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindCompute(cmd);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd,
                {ssaoIn.GetDescription().Width, ssaoIn.GetDescription().Height, 1},
                {16, 16, 1});
        });
}
