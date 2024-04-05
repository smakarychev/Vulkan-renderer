#include "HiZVisualize.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "Vulkan/RenderCommand.h"

HiZVisualize::HiZVisualize(RenderGraph::Graph& renderGraph)
{
    ShaderPipelineTemplate* hizVisualizeTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
            "../assets/shaders/processed/render-graph/common/fullscreen-vert.shader",
            "../assets/shaders/processed/render-graph/culling/hiz-visualize-frag.shader"},
        "Pass.HiZ.Visualize", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(hizVisualizeTemplate)
        .SetRenderingDetails({
           .ColorFormats = {Format::RGBA16_FLOAT}})
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(hizVisualizeTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(hizVisualizeTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void HiZVisualize::AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource hiz)
{
    using namespace RenderGraph;

    static ShaderDescriptors::BindingInfo samplerBindingInfo =
        m_PipelineData.SamplerDescriptors.GetBindingInfo("u_sampler");
    static ShaderDescriptors::BindingInfo hizBindingInfo =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_hiz");

    m_Pass = &renderGraph.AddRenderPass<PassData>({"HiZ.Visualize"},
        [&](Graph& graph, PassData& passData)
        {
            passData.HiZ = graph.Read(hiz,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Sampled);
            const TextureDescription& hizDescription = graph.GetTextureDescription(hiz);

            passData.ColorOut = graph.CreateResource("HiZ.Visualize.ColorOut", GraphTextureDescription{
                .Width = hizDescription.Width,
                .Height = hizDescription.Height,
                .Format = Format::RGBA16_FLOAT});
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Load, AttachmentStore::Store);

            passData.PipelineData = &m_PipelineData;
            passData.PushConstants = &m_PushConstants;

            graph.GetBlackboard().Update(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("HiZ visualize")
            
            const Texture& hizTexture = resources.GetTexture(passData.HiZ);
            auto& pushConstants = *passData.PushConstants;
            ImGui::Begin("HiZ visualize");
            ImGui::DragInt("mip level", (i32*)&pushConstants.MipLevel, 1.0f, 0, 10);            
            ImGui::DragFloat("intensity", &pushConstants.IntensityScale, 10.0f, 1.0f, 1e+4f);            
            ImGui::End();

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;
            
            samplerDescriptors.UpdateBinding(samplerBindingInfo,
                hizTexture.BindingInfo(ImageFilter::Nearest, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding(hizBindingInfo,
                hizTexture.BindingInfo(ImageFilter::Nearest, ImageLayout::Readonly));

            pipeline.BindGraphics(frameContext.Cmd);
            RenderCommand::PushConstants(frameContext.Cmd, pipeline.GetLayout(), pushConstants);
            samplerDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());
            resourceDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());

            RenderCommand::Draw(frameContext.Cmd, 3);
        });
}
