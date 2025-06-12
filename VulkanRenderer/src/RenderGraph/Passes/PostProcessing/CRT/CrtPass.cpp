#include "CrtPass.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/CrtBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::Crt::PassData& Passes::Crt::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource colorIn,
    RG::Resource colorTarget)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct SettingsUBO
    {
        f32 Curvature{0.2f};
        f32 ColorSplit{0.004f};
        f32 LinesMultiplier{1.0f};
        f32 VignettePower{0.64f};
        f32 VignetteRadius{0.025f};
    };
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("CRT.Setup")

            graph.SetShader("crt"_hsv);
            
            passData.ColorIn = graph.ReadImage(colorIn, Pixel | Sampled);
            
            passData.ColorOut = graph.RenderTarget(colorTarget, {});

            auto& globalResources = graph.GetGlobalResources();
            
            passData.Time = graph.Create("Time"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(f32)});
            passData.Time = graph.ReadBuffer(passData.Time, Pixel | Uniform);
            passData.Time = graph.Upload(passData.Time, (f32)globalResources.FrameNumberTick);

            passData.Settings = graph.Create("Settings"_hsv, RGBufferDescription{
                .SizeBytes = sizeof(SettingsUBO)});
            passData.Settings = graph.ReadBuffer(passData.Settings, Pixel | Uniform);
            auto& settings = graph.GetOrCreateBlackboardValue<SettingsUBO>();
            ImGui::Begin("CRT settings");
            ImGui::DragFloat("curvature", &settings.Curvature, 1e-2f, 0.0f, 10.0f);            
            ImGui::DragFloat("color split", &settings.ColorSplit, 1e-4f, 0.0f, 0.5f);            
            ImGui::DragFloat("lines multiplier", &settings.LinesMultiplier, 1e-1f, 0.0f, 10.0f);            
            ImGui::DragFloat("vignette power", &settings.VignettePower, 1e-2f, 0.0f, 5.0f);            
            ImGui::DragFloat("vignette clear radius", &settings.VignetteRadius, 1e-3f, 0.0f, 1.0f);            
            ImGui::End();
            passData.Settings = graph.Upload(passData.Settings, settings);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("CRT")
            GPU_PROFILE_FRAME("CRT")
            
            const Shader& shader = graph.GetShader();
            CrtShaderBindGroup bindGroup(shader);
            bindGroup.SetSampler(Device::CreateSampler({}));
            bindGroup.SetImage(graph.GetImageBinding(passData.ColorIn));
            bindGroup.SetTime(graph.GetBufferBinding(passData.Time));
            bindGroup.SetSettings(graph.GetBufferBinding(passData.Settings));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
