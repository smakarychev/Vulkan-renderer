#include "CrtPass.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/Shader.h"
#include "Vulkan/RenderCommand.h"

CrtPass::CrtPass(RG::Graph& renderGraph)
{
    ShaderPipelineTemplate* crtTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/common/fullscreen-vert.shader",
          "../assets/shaders/processed/render-graph/post/crt-frag.shader"},
      "Pass.CRT", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
       .SetTemplate(crtTemplate)
       .SetRenderingDetails({
           .ColorFormats = {Format::RGBA16_FLOAT}})
       .UseDescriptorBuffer()
       .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(crtTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(crtTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void CrtPass::AddToGraph(RG::Graph& renderGraph, RG::Resource colorIn,
    RG::Resource colorTarget)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    static ShaderDescriptors::BindingInfo samplerBindingInfo =
        m_PipelineData.SamplerDescriptors.GetBindingInfo("u_sampler");
    static ShaderDescriptors::BindingInfo imageBindingInfo =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_image");
    static ShaderDescriptors::BindingInfo timeBindingInfo = 
    m_PipelineData.ResourceDescriptors.GetBindingInfo("u_time");
    static ShaderDescriptors::BindingInfo settingsBindingInfo =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_settings");

    m_Pass = &renderGraph.AddRenderPass<PassData>({"CRT"},
        [&](Graph& graph, PassData& passData)
        {
            passData.ColorIn = graph.Read(colorIn, Pixel | Sampled);
            
            passData.ColorOut = graph.RenderTarget(colorTarget,
                AttachmentLoad::Load, AttachmentStore::Store);

            passData.TimeUbo = graph.CreateResource("CRT.Time", GraphBufferDescription{
                .SizeBytes = sizeof(f32)});
            passData.TimeUbo = graph.Read(passData.TimeUbo, Pixel | Uniform | Upload);

            passData.SettingsUbo = graph.CreateResource("CRT.Settings", GraphBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.SettingsUbo = graph.Read(passData.SettingsUbo, Pixel | Uniform | Upload);

            passData.PipelineData = &m_PipelineData;
            passData.Settings = &m_SettingsUBO;

            graph.GetBlackboard().Register(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("CRT post-processing")
            const Texture& colorInTexture = resources.GetTexture(passData.ColorIn);
            const Buffer& time = resources.GetBuffer(passData.TimeUbo, (f32)frameContext.FrameNumberTick,
                *frameContext.ResourceUploader);
            
            auto& settingsUBO = *passData.Settings;
            ImGui::Begin("CRT settings");
            ImGui::DragFloat("curvature", &settingsUBO.Curvature, 1e-2f, 0.0f, 10.0f);            
            ImGui::DragFloat("color split", &settingsUBO.ColorSplit, 1e-4f, 0.0f, 0.5f);            
            ImGui::DragFloat("lines multiplier", &settingsUBO.LinesMultiplier, 1e-1f, 0.0f, 10.0f);            
            ImGui::DragFloat("vignette power", &settingsUBO.VignettePower, 1e-2f, 0.0f, 5.0f);            
            ImGui::DragFloat("vignette clear radius", &settingsUBO.VignetteRadius, 1e-3f, 0.0f, 1.0f);            
            ImGui::End();
            const Buffer& settings = resources.GetBuffer(passData.SettingsUbo, settingsUBO,
                *frameContext.ResourceUploader);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;
            
            samplerDescriptors.UpdateBinding(samplerBindingInfo,
                colorInTexture.BindingInfo(ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding(imageBindingInfo,
                colorInTexture.BindingInfo(ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding(timeBindingInfo, time.BindingInfo());
            resourceDescriptors.UpdateBinding(settingsBindingInfo, settings.BindingInfo());
            
            pipeline.BindGraphics(frameContext.Cmd);
            samplerDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());
            resourceDescriptors.BindGraphics(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());

            RenderCommand::Draw(frameContext.Cmd, 3);
        });
}