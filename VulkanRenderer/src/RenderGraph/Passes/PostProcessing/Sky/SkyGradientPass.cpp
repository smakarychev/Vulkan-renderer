#include "SkyGradientPass.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "Vulkan/RenderCommand.h"

SkyGradientPass::SkyGradientPass(RG::Graph& renderGraph)
{
    ShaderPipelineTemplate* skyTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
          "../assets/shaders/processed/render-graph/post/sky-gradient-comp.shader"},
      "Pass.SkyGradient", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
       .SetTemplate(skyTemplate)
       .UseDescriptorBuffer()
       .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(skyTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(0)
        .Build();
}

void SkyGradientPass::AddToGraph(RG::Graph& renderGraph, RG::Resource renderTarget)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    static ShaderDescriptors::BindingInfo cameraBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_camera");
    static ShaderDescriptors::BindingInfo settingsBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_settings");
    static ShaderDescriptors::BindingInfo imageOutBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_out_image");
    
    m_Pass = &renderGraph.AddRenderPass<PassData>({"SkyGradient"},
        [&](Graph& graph, PassData& passData)
        {
            passData.Camera = graph.CreateResource("SkyGradient.Camera", GraphBufferDescription{
                .SizeBytes = sizeof(passData.Camera)});
            passData.Camera = graph.Read(passData.Camera, Compute | Uniform | Upload);
            
            passData.Settings = graph.CreateResource("SkyGradient.Settings", GraphBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.Settings = graph.Read(passData.Settings, Compute | Uniform | Upload);

            passData.ColorOut = graph.Write(renderTarget, Compute | Storage);

            passData.PipelineData = &m_PipelineData;
            passData.SettingsData = &m_Settings;

            graph.GetBlackboard().Register(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("sky gradient")
            
            passData.CameraData.ViewInverse = glm::inverse(frameContext.MainCamera->GetView());
            passData.CameraData.Position = frameContext.MainCamera->GetPosition();

            auto& settings = *passData.SettingsData;
            ImGui::Begin("Sky gradient");
            ImGui::ColorEdit3("sky horizon", (f32*)&settings.SkyColorHorizon);
            ImGui::ColorEdit3("sky zenith", (f32*)&settings.SkyColorZenith);
            ImGui::ColorEdit3("ground", (f32*)&settings.GroundColor);
            ImGui::DragFloat("horizon to zenith", &settings.HorizonToZenithWidth, 1e-3f, 0.0f, 1.0f);
            ImGui::DragFloat("ground to horizon", &settings.GroundToSkyWidth, 1e-3f, 0.0f, 1.0f);
            ImGui::DragFloat("horizon rate", &settings.HorizonToZenithRate, 1e-1f, 1.0f, 100.0f);
            ImGui::DragFloat("ground rate", &settings.GroundToSkyRate, 1e-1f, 1.0f, 100.0f);
            ImGui::DragFloat3("sun direction", (f32*)&settings.SunDirection, 1e-3f, -1.0f, -1.0f);
            ImGui::DragFloat("sun radius", &settings.SunRadius, 1e-1f, 1.0f, 1024.0f);
            ImGui::DragFloat("sun intensity", &settings.SunIntensity, 1e-3f, 0.0f, 1.0f);
            ImGui::End();
            
            const Buffer& camera = resources.GetBuffer(passData.Camera, passData.Camera,
                *frameContext.ResourceUploader);
            const Buffer& settingsBuffer = resources.GetBuffer(passData.Settings, settings,
                *frameContext.ResourceUploader);
            const Texture& colorOut = resources.GetTexture(passData.ColorOut);

            passData.PushConstants.ImageSize = {colorOut.Description().Width, colorOut.Description().Height};

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;
            
            resourceDescriptors.UpdateBinding(cameraBinding, camera.BindingInfo());
            resourceDescriptors.UpdateBinding(settingsBinding, settingsBuffer.BindingInfo());
            resourceDescriptors.UpdateBinding(imageOutBinding,
                colorOut.BindingInfo(ImageFilter::Linear, ImageLayout::General));

            pipeline.BindCompute(frameContext.Cmd);
            RenderCommand::PushConstants(frameContext.Cmd, pipeline.GetLayout(), passData.PushConstants);
            resourceDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());
            RenderCommand::Dispatch(frameContext.Cmd, {passData.PushConstants.ImageSize, 1}, {32, 32, 1});
        });
}
