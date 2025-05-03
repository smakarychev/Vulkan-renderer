#include "HiZVisualize.h"

#include "Renderer.h"
#include "imgui/imgui.h"
#include "RenderGraph/Passes/Generated/HizVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::HiZVisualize::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource hiz)
{
    using namespace RG;

    struct PushConstants
    {
        u32 MipLevel{0};
        f32 IntensityScale{10.0f};
    };

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("HiZ.Visualize.Setup")

            graph.SetShader("hiz-visualize"_hsv);
            
            passData.HiZ = graph.Read(hiz,
                ResourceAccessFlags::Pixel | ResourceAccessFlags::Sampled);
            const TextureDescription& hizDescription = graph.GetTextureDescription(hiz);

            passData.ColorOut = graph.CreateResource("ColorOut"_hsv, GraphTextureDescription{
                .Width = hizDescription.Width,
                .Height = hizDescription.Height,
                .Format = Format::RGBA16_FLOAT});
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                AttachmentLoad::Load, AttachmentStore::Store);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("HiZ.Visualize")
            GPU_PROFILE_FRAME("HiZ.Visualize")
            
            Texture hizTexture = resources.GetTexture(passData.HiZ);
            PushConstants& pushConstants = resources.GetOrCreateValue<PushConstants>();
            ImGui::Begin("HiZ visualize");
            ImGui::DragInt("mip level", (i32*)&pushConstants.MipLevel, 1.0f, 0, 10);            
            ImGui::DragFloat("intensity", &pushConstants.IntensityScale, 10.0f, 1.0f, 1e+4f);            
            ImGui::End();

            const Shader& shader = resources.GetGraph()->GetShader();
            HizVisualizeShaderBindGroup bindGroup(shader);
            bindGroup.SetSampler(Device::CreateSampler({
                .MinificationFilter = ImageFilter::Nearest,
                .MagnificationFilter = ImageFilter::Nearest}));
            bindGroup.SetHiz({.Image = hizTexture}, ImageLayout::Readonly);

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstants}});
            cmd.Draw({.VertexCount = 3});
        });

    return pass;
}
