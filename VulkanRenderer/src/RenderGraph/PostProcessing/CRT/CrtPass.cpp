#include "CrtPass.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/Shader.h"
#include "Vulkan/RenderCommand.h"

CrtPass::CrtPass(RenderGraph::Graph& renderGraph, RenderGraph::Resource colorIn, RenderGraph::Resource colorTarget)
{
    using namespace RenderGraph;

    ShaderPipelineTemplate* crtTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/fullscreen-vert.shader",
          "../assets/shaders/processed/render-graph/crt-frag.shader"},
      "render-graph-ctr-pass-template", renderGraph.GetArenaAllocators());

    ShaderPipeline crtPipeline = ShaderPipeline::Builder()
       .SetTemplate(crtTemplate)
       .SetRenderingDetails({
           .ColorFormats = {Format::RGBA16_FLOAT}})
       .UseDescriptorBuffer()
       .Build();

    ShaderDescriptors samplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(crtTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    ShaderDescriptors resourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(crtTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
    
    ShaderDescriptors::BindingInfo samplerBindingInfo = samplerDescriptors.GetBindingInfo("u_sampler");
    ShaderDescriptors::BindingInfo imageBindingInfo = resourceDescriptors.GetBindingInfo("u_image");
    ShaderDescriptors::BindingInfo timeBindingInfo = resourceDescriptors.GetBindingInfo("u_time");
    ShaderDescriptors::BindingInfo settingsBindingInfo = resourceDescriptors.GetBindingInfo("u_settings");

    m_Pass = &renderGraph.AddRenderPass<PassData>("crt-pass",
        [&](Graph& graph, PassData& passData)
        {
            passData.ColorIn = graph.Read(colorIn,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Sampled);
            
            passData.ColorTarget = graph.RenderTarget(colorTarget,
                AttachmentLoad::Load, AttachmentStore::Store);

            passData.TimeUbo = graph.CreateResource("crt-pass-time", GraphBufferDescription{
                .SizeBytes = sizeof(f32)});
            passData.TimeUbo = graph.Read(passData.TimeUbo,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Uniform);

            passData.SettingsUbo = graph.CreateResource("crt-pass-settings", GraphBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.SettingsUbo = graph.Read(passData.SettingsUbo,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Uniform);

            graph.GetBlackboard().RegisterOutput(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("CRT post-processing")
            const Texture& colorInTexture = resources.GetTexture(passData.ColorIn);
            const Buffer& time = resources.GetBuffer(passData.TimeUbo, (f32)frameContext.FrameNumberTick,
                *frameContext.ResourceUploader);
            
            auto& settingsUBO = passData.Settings;
            ImGui::Begin("CRT settings");
            ImGui::DragFloat("curvature", &passData.Settings.Curvature, 1e-2f, 0.0f, 10.0f);            
            ImGui::DragFloat("color split", &passData.Settings.ColorSplit, 1e-4f, 0.0f, 0.5f);            
            ImGui::DragFloat("lines multiplier", &passData.Settings.LinesMultiplier, 1e-1f, 0.0f, 10.0f);            
            ImGui::DragFloat("vignette power", &passData.Settings.VignettePower, 1e-2f, 0.0f, 5.0f);            
            ImGui::DragFloat("vignette clear radius", &passData.Settings.VignetteRadius, 1e-2f, 0.0f, 1.0f);            
            ImGui::End();
            const Buffer& settings = resources.GetBuffer(passData.SettingsUbo, settingsUBO,
                *frameContext.ResourceUploader);

            samplerDescriptors.UpdateBinding(samplerBindingInfo,
                colorInTexture.CreateBindingInfo(ImageFilter::Linear, ImageLayout::ReadOnly));
            resourceDescriptors.UpdateBinding(imageBindingInfo,
                colorInTexture.CreateBindingInfo(ImageFilter::Linear, ImageLayout::ReadOnly));
            resourceDescriptors.UpdateBinding(timeBindingInfo, time.CreateBindingInfo());
            resourceDescriptors.UpdateBinding(settingsBindingInfo, settings.CreateBindingInfo());
            
            crtPipeline.BindGraphics(frameContext.Cmd);
            samplerDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                crtPipeline.GetLayout());
            resourceDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                crtPipeline.GetLayout());

            RenderCommand::Draw(frameContext.Cmd, 3);
        });

}
