#include "VisualizeBRDFPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "Vulkan/RenderCommand.h"

VisualizeBRDFPass::VisualizeBRDFPass(RG::Graph& renderGraph)
{
    ShaderPipelineTemplate* brdfTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/common/fullscreen-vert.shader",
        "../assets/shaders/processed/render-graph/pbr/visualize-brdf-frag.shader"},
        "Pass.BRDF.Visualize", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(brdfTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT}})
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(brdfTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();
        
    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(brdfTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void VisualizeBRDFPass::AddToGraph(RG::Graph& renderGraph, const Texture& brdf, RG::Resource colorIn,
    const glm::uvec2 resolution)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{"BRDF.Visualize"},
        [&](Graph& graph, PassData& passData)
        {
            passData.ColorOut = RG::RgUtils::ensureResource(colorIn, graph, "BRDF.Visualize.ColorOut",
                GraphTextureDescription{
                    .Width = resolution.x,
                    .Height = resolution.y,
                    .Format = Format::RGBA16_FLOAT});

            passData.BRDF = graph.AddExternal("BRDF.Visualize.BRDF", brdf);

            passData.BRDF = graph.Read(passData.BRDF, Pixel | Sampled);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Load, AttachmentStore::Store);
            
            passData.PipelineData = &m_PipelineData;

            Sampler brdfSampler = Sampler::Builder()
                .Filters(ImageFilter::Linear, ImageFilter::Linear)
                .WrapMode(SamplerWrapMode::ClampBorder)
                .Build();

            passData.BRDFSampler = brdfSampler;

            graph.GetBlackboard().Register(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {   
            GPU_PROFILE_FRAME("BRDF Visualize");

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding("u_sampler", brdf.BindingInfo(passData.BRDFSampler,
                ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_brdf", brdf.BindingInfo(passData.BRDFSampler, ImageLayout::Readonly));

            auto& cmd = frameContext.Cmd;
            pipeline.BindGraphics(cmd);
            samplerDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Draw(cmd, 3);
        });
}
