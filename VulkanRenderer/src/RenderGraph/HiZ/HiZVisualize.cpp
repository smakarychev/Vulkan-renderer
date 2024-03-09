#include "HiZVisualize.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "Vulkan/RenderCommand.h"

HiZVisualize::HiZVisualize(RenderGraph::Graph& renderGraph, RenderGraph::Resource hiz)
{
    using namespace RenderGraph;

    ShaderPipelineTemplate* hizVisualizeTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/fullscreen-vert.shader",
          "../assets/shaders/processed/render-graph/hiz-visualize-frag.shader"},
      "render-graph-hiz-visualize-pass-template", renderGraph.GetArenaAllocators());

    ShaderPipeline pipeline = ShaderPipeline::Builder()
       .SetTemplate(hizVisualizeTemplate)
       .SetRenderingDetails({
           .ColorFormats = {Format::RGBA16_FLOAT}})
       .UseDescriptorBuffer()
       .Build();

    ShaderDescriptors samplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(hizVisualizeTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    ShaderDescriptors resourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(hizVisualizeTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();

    ShaderDescriptors::BindingInfo samplerBindingInfo = samplerDescriptors.GetBindingInfo("u_sampler");
    ShaderDescriptors::BindingInfo hizBindingInfo = resourceDescriptors.GetBindingInfo("u_hiz");

    m_Pass = &renderGraph.AddRenderPass<PassData>("hiz-visualize-pass",
        [&](Graph& graph, PassData& passData)
        {
            passData.HiZ = graph.Read(hiz,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Sampled);
            const TextureDescription& hizDescription = graph.GetTextureDescription(hiz);

            passData.ColorOut = graph.CreateResource("hiz-visualize-out", GraphTextureDescription{
                .Width = hizDescription.Width,
                .Height = hizDescription.Height,
                .Format = Format::RGBA16_FLOAT});
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Load, AttachmentStore::Store);

            graph.GetBlackboard().RegisterOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("HiZ visualize")
            
            const Texture& hizTexture = resources.GetTexture(passData.HiZ);
            ImGui::Begin("HiZ visualize");
            ImGui::DragInt("mip level", (i32*)&passData.PushConstants.MipLevel, 1.0f, 0, 10);            
            ImGui::DragFloat("intensity", &passData.PushConstants.IntensityScale, 10.0f, 1.0f, 1e+4f);            
            ImGui::End();
            samplerDescriptors.UpdateBinding(samplerBindingInfo,
                hizTexture.CreateBindingInfo(ImageFilter::Nearest, ImageLayout::ReadOnly));
            resourceDescriptors.UpdateBinding(hizBindingInfo,
                hizTexture.CreateBindingInfo(ImageFilter::Nearest, ImageLayout::ReadOnly));

            pipeline.BindGraphics(frameContext.Cmd);
            RenderCommand::PushConstants(frameContext.Cmd, pipeline.GetLayout(), passData.PushConstants);
            samplerDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());
            resourceDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());

            RenderCommand::Draw(frameContext.Cmd, 3);
        });
}
