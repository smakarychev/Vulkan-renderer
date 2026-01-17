#include "rendererpch.h"

#include "HiZVisualize.h"

#include "imgui/imgui.h"
#include "RenderGraph/Passes/Generated/HizVisualizeBindGroupRG.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::HiZVisualize::PassData& Passes::HiZVisualize::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, HizVisualizeBindGroupRG>;

    struct PushConstants
    {
        u32 MipLevel{0};
        f32 IntensityScale{10.0f};
    };

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("HiZ.Visualize.Setup")

            passData.BindGroup = HizVisualizeBindGroupRG(graph);

            passData.Color = graph.Create("ColorOut"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size,
                .Reference = info.Hiz,
                .Format = passData.BindGroup.GetHizAttachmentFormat()
            });
            passData.Color = graph.RenderTarget(passData.Color, {});
            passData.BindGroup.SetResourcesHiz(info.Hiz);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("HiZ.Visualize")
            GPU_PROFILE_FRAME("HiZ.Visualize")

            PushConstants& pushConstants = graph.GetOrCreateBlackboardValue<PushConstants>();
            ImGui::Begin("HiZ visualize");
            ImGui::DragInt("mip level", (i32*)&pushConstants.MipLevel, 1.0f, 0, 10);
            ImGui::DragFloat("intensity", &pushConstants.IntensityScale, 10.0f, 1.0f, 1e+4f);
            ImGui::End();

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(),
                .Data = {pushConstants}
            });
            cmd.Draw({.VertexCount = 3});
        });
}
