#include "VisualizeDepthPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "Vulkan/RenderCommand.h"

VisualizeDepthPass::VisualizeDepthPass(RG::Graph& renderGraph, std::string_view name)
    : m_Name(name)
{
    ShaderPipelineTemplate* depthTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
       "../assets/shaders/processed/render-graph/common/fullscreen-vert.shader",
       "../assets/shaders/processed/render-graph/general/visualize-depth-frag.shader"},
       "Pass.Depth.Visualize", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(depthTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT}})
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(depthTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(depthTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void VisualizeDepthPass::AddToGraph(RG::Graph& renderGraph, RG::Resource depthIn, RG::Resource colorIn,
    f32 near, f32 far, bool isOrthographic)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    std::string name = m_Name.Name();
    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            auto& depthDescription = Resources(graph).GetTextureDescription(depthIn);
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, name + ".Color",
                GraphTextureDescription{
                    .Width = depthDescription.Width,
                    .Height = depthDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.DepthIn = graph.Read(depthIn, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, AttachmentLoad::Load, AttachmentStore::Store);

            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().Update(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Visualize Depth")

            const Texture& depthTexture = resources.GetTexture(passData.DepthIn);

            auto& pipeline = passData.PipelineData->Pipeline;    
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;    
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

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
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            
            RenderCommand::Draw(cmd, 3);
        });
}
