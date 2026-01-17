#include "rendererpch.h"

#include "CrtPass.h"

#include "imgui/imgui.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Generated/CrtBindGroupRG.generated.h"

Passes::Crt::PassData& Passes::Crt::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, CrtBindGroupRG>;
    using enum ResourceAccessFlags;

    struct SettingsUBO
    {
        f32 Curvature{0.2f};
        f32 ColorSplit{0.004f};
        f32 LinesMultiplier{1.0f};
        f32 VignettePower{0.64f};
        f32 VignetteRadius{0.025f};
    };
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("CRT.Setup")

            passData.BindGroup = CrtBindGroupRG(graph);
            
            passData.Color = graph.RenderTarget(graph.Create("Color.Accumulation.In"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d | RGImageInference::Format,
                .Reference = info.Color,
            }), {});
            passData.BindGroup.SetResourcesImage(info.Color);
            
            auto& globalResources = graph.GetGlobalResources();
            Resource time = passData.BindGroup.SetResourcesTime(
                graph.Create("Time"_hsv, RGBufferDescription{.SizeBytes = sizeof(f32)}));
            graph.Upload(time, (f32)globalResources.FrameNumberTick);

            Resource settings = passData.BindGroup.SetResourcesSettings(
                graph.Create("Settings"_hsv, RGBufferDescription{.SizeBytes = sizeof(SettingsUBO)}));
            auto& settingsUbo = graph.GetOrCreateBlackboardValue<SettingsUBO>();
            ImGui::Begin("CRT settings");
            ImGui::DragFloat("curvature", &settingsUbo.Curvature, 1e-2f, 0.0f, 10.0f);            
            ImGui::DragFloat("color split", &settingsUbo.ColorSplit, 1e-4f, 0.0f, 0.5f);            
            ImGui::DragFloat("lines multiplier", &settingsUbo.LinesMultiplier, 1e-1f, 0.0f, 10.0f);            
            ImGui::DragFloat("vignette power", &settingsUbo.VignettePower, 1e-2f, 0.0f, 5.0f);            
            ImGui::DragFloat("vignette clear radius", &settingsUbo.VignetteRadius, 1e-3f, 0.0f, 1.0f);            
            ImGui::End();
            graph.Upload(settings, settingsUbo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("CRT")
            GPU_PROFILE_FRAME("CRT")
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.Draw({.VertexCount = 3});
        });
}
