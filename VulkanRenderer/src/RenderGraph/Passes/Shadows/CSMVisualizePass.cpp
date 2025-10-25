#include "rendererpch.h"

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
            
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, "ColorOut"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Size2d,
                    .Reference = csmTexture,
                    .Format = Format::RGBA16_FLOAT});

            passData.ShadowMap = graph.ReadImage(csmTexture, Pixel | Sampled);
            passData.CSM = graph.ReadBuffer(csmInfo, Pixel | Uniform);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {
                .OnLoad = colorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                .ClearColor = {.F = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}}
            });
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("CSM.Visualize")
            GPU_PROFILE_FRAME("CSM.Visualize")

            const Shader& shader = graph.GetShader();
            CsmVisualizeShaderBindGroup bindGroup(shader);

            bindGroup.SetShadowMap(graph.GetImageBinding(passData.ShadowMap));
            bindGroup.SetCsmData(graph.GetBufferBinding(passData.CSM));

            auto& cascadeIndex = graph.GetOrCreateBlackboardValue<CascadeIndex>();
            ImGui::Begin("CSM Visualize");
            ImGui::DragInt("CSM cascade", (i32*)&cascadeIndex.Index, 1e-1f, 0, SHADOW_CASCADES);
            ImGui::End();
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {cascadeIndex.Index}});
            cmd.Draw({.VertexCount = 3});
        });
}
