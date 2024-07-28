#include "SkyGradientPass.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::SkyGradient::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource renderTarget)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct SettingsUBO
    {
        glm::vec4 SkyColorHorizon{0.42f, 0.66f, 0.66f, 1.0f};
        glm::vec4 SkyColorZenith{0.12f, 0.32f, 0.54f, 1.0f};
        glm::vec4 GroundColor{0.21f, 0.21f, 0.11f, 1.0f};
        glm::vec4 SunDirection{0.1f, -0.1f, 0.1f, 0.0f};
        f32 SunRadius{128.0f};
        f32 SunIntensity{0.5f};
        f32 GroundToSkyWidth{0.01f};
        f32 HorizonToZenithWidth{0.35f};
        f32 GroundToSkyRate{2.7f};
        f32 HorizonToZenithRate{2.5f};
    };
    struct PushConstants
    {
        glm::uvec2 ImageSize;
    };

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Sky.Gradient.Setup")

            graph.SetShader("../assets/shaders/sky-gradient.shader");

            passData.Camera = graph.CreateResource("SkyGradient.Camera", GraphBufferDescription{
                .SizeBytes = sizeof(passData.Camera)});
            passData.Camera = graph.Read(passData.Camera, Compute | Uniform | Upload);
            
            passData.Settings = graph.CreateResource("SkyGradient.Settings", GraphBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.Settings = graph.Read(passData.Settings, Compute | Uniform | Upload);

            passData.ColorOut = graph.Write(renderTarget, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Sky.Gradient")
            GPU_PROFILE_FRAME("Sky.Gradient")

            auto& settings = resources.GetOrCreateValue<SettingsUBO>();
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

            glm::uvec2 imageSize = {colorOut.Description().Width, colorOut.Description().Height};

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);
            
            resourceDescriptors.UpdateBinding("u_camera", camera.BindingInfo());
            resourceDescriptors.UpdateBinding("u_settings", settingsBuffer.BindingInfo());
            resourceDescriptors.UpdateBinding("u_out_image",
                colorOut.BindingInfo(ImageFilter::Linear, ImageLayout::General));

            pipeline.BindCompute(frameContext.Cmd);
            RenderCommand::PushConstants(frameContext.Cmd, pipeline.GetLayout(), imageSize);
            resourceDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                pipeline.GetLayout());
            RenderCommand::Dispatch(frameContext.Cmd, {imageSize, 1}, {32, 32, 1});
        });

    return pass;
}
