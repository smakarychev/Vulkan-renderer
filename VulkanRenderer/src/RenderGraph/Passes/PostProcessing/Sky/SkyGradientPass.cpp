#include "SkyGradientPass.h"

#include "ViewInfoGPU.h"
#include "Renderer.h"
#include "imgui/imgui.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Generated/SkyGradientBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::SkyGradient::PassData& Passes::SkyGradient::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource renderTarget)
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

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Sky.Gradient.Setup")

            graph.SetShader("sky-gradient"_hsv);

            auto& globalResources = graph.GetGlobalResources();
            
            passData.ViewInfo = graph.ReadBuffer(globalResources.PrimaryViewInfoResource, Compute | Uniform);
            
            passData.Settings = graph.Create("Settings"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.Settings = graph.ReadBuffer(passData.Settings, Compute | Uniform);
            auto& settings = graph.GetOrCreateBlackboardValue<SettingsUBO>();
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
            passData.Settings = graph.Upload(passData.Settings, settings);

            passData.ColorOut = graph.WriteImage(renderTarget, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Sky.Gradient")
            GPU_PROFILE_FRAME("Sky.Gradient")

            auto& colorOutDescription = graph.GetImageDescription(passData.ColorOut);

            glm::uvec2 imageSize = {colorOutDescription.Width, colorOutDescription.Height};

            const Shader& shader = graph.GetShader();
            SkyGradientShaderBindGroup bindGroup(shader);
            bindGroup.SetCamera(graph.GetBufferBinding(passData.ViewInfo));
            bindGroup.SetSettings(graph.GetBufferBinding(passData.Settings));
            bindGroup.SetOutImage(graph.GetImageBinding(passData.ColorOut));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {imageSize}});
            cmd.Dispatch({
                .Invocations = {imageSize, 1},
                .GroupSize = {32, 32, 1}});
        });
}
