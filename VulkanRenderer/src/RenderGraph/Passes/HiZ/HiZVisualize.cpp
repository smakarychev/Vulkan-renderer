#include "rendererpch.h"

#include "HiZVisualize.h"

#include "imgui/imgui.h"
#include "RenderGraph/Passes/Generated/HizVisualizeBindGroup.generated.h"
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

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("HiZ.Visualize.Setup")

            graph.SetShader("hiz-visualize"_hsv);
            
            passData.HiZ = graph.ReadImage(hiz,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Sampled);
            passData.ColorOut = graph.Create("ColorOut"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size,
                .Reference = hiz,
                .Format = Format::RGBA16_FLOAT});
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {});
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("HiZ.Visualize")
            GPU_PROFILE_FRAME("HiZ.Visualize")
            
            PushConstants& pushConstants = graph.GetOrCreateBlackboardValue<PushConstants>();
            ImGui::Begin("HiZ visualize");
            ImGui::DragInt("mip level", (i32*)&pushConstants.MipLevel, 1.0f, 0, 10);            
            ImGui::DragFloat("intensity", &pushConstants.IntensityScale, 10.0f, 1.0f, 1e+4f);            
            ImGui::End();

            const Shader& shader = graph.GetShader();
            HizVisualizeShaderBindGroup bindGroup(shader);
            bindGroup.SetSampler(Device::CreateSampler({
                .MinificationFilter = ImageFilter::Nearest,
                .MagnificationFilter = ImageFilter::Nearest}));
            bindGroup.SetHiz(graph.GetImageBinding(passData.HiZ));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstants}});
            cmd.Draw({.VertexCount = 3});
        });
}
