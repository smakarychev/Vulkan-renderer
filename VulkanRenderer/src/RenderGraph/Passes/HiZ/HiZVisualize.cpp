#include "rendererpch.h"

#include "HiZVisualize.h"

#include "imgui/imgui.h"
#include "RenderGraph/Passes/Generated/HizVisualizeBindGroupRG.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::HiZVisualize::PassData& Passes::HiZVisualize::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource hiz)
{
    using namespace RG;

    struct PushConstants
    {
        u32 MipLevel{0};
        f32 IntensityScale{10.0f};
    };

    using PassDataBind = PassDataWithBind<PassData, HizVisualizeBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("HiZ.Visualize.Setup")

            passData.BindGroup = HizVisualizeBindGroupRG(graph, graph.SetShader("hizVisualize"_hsv));

            passData.HiZ = passData.BindGroup.SetResourcesHiz(hiz);
            passData.ColorOut = graph.Create("ColorOut"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size,
                .Reference = hiz,
                .Format = passData.BindGroup.GetHizAttachmentFormat()});
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {});
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
            passData.BindGroup.BindGraphics(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
                .Data = {pushConstants}});
            cmd.Draw({.VertexCount = 3});
        });
}
