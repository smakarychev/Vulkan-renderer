#include "CSMVisualizePass.h"

#include "FrameContext.h"
#include "imgui/imgui.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/CsmVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::VisualizeCSM::PassData& Passes::VisualizeCSM::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource csmTexture, RG::Resource csmInfo, RG::Resource colorIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct CascadeIndex
    {
        u32 Index{0};
    };
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("CSM.Visualize.Setup")

            graph.SetShader("csm-visualize"_hsv);
            
            const TextureDescription& csmDescription = Resources(graph).GetTextureDescription(csmTexture);
            
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, "ColorOut"_hsv,
                GraphTextureDescription{
                    .Width = csmDescription.Width,
                    .Height = csmDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.ShadowMap = graph.Read(csmTexture, Pixel | Sampled);
            passData.CSM = graph.Read(csmInfo, Pixel | Uniform);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                colorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                AttachmentStore::Store, {.F = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}});
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("CSM.Visualize")
            GPU_PROFILE_FRAME("CSM.Visualize")

            auto&& [shadowMap, shadowDescription] = resources.GetTextureWithDescription(passData.ShadowMap);
            Buffer csmData = resources.GetBuffer(passData.CSM);

            const Shader& shader = resources.GetGraph()->GetShader();
            CsmVisualizeShaderBindGroup bindGroup(shader);

            bindGroup.SetShadowMap({.Image = shadowMap},
                shadowDescription.Format == Format::D32_FLOAT ?
                    ImageLayout::DepthReadonly : ImageLayout::DepthStencilReadonly);
            bindGroup.SetCsmData({.Buffer = csmData});

            auto& cascadeIndex = resources.GetOrCreateValue<CascadeIndex>();
            ImGui::Begin("CSM Visualize");
            ImGui::DragInt("CSM cascade", (i32*)&cascadeIndex.Index, 1e-1f, 0, SHADOW_CASCADES);
            ImGui::End();
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {cascadeIndex.Index}});
            cmd.Draw({.VertexCount = 3});
        }).Data;
}
