#include "rendererpch.h"

#include "CsmVisualizePass.h"

#include "FrameContext.h"
#include "imgui/imgui.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/TextureArrayToSliceBindGroupRG.generated.h"
#include "RenderGraph/Passes/Utility/ChannelCompositionInfo.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::VisualizeCsm::PassData& Passes::VisualizeCsm::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource csmTexture)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct CascadeIndex
    {
        u32 Index{0};
    };

    using PassDataBind = PassDataWithBind<PassData, TextureArrayToSliceBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("CSM.Visualize.Setup")

            passData.BindGroup = TextureArrayToSliceBindGroupRG(graph, graph.SetShader("textureArrayToSlice"_hsv,
                ShaderDefines({
                    ShaderDefine{"G_CHANNEL"_hsv, (u32)Channel::R},
                    ShaderDefine{"B_CHANNEL"_hsv, (u32)Channel::R},
                    ShaderDefine{"A_CHANNEL"_hsv, (u32)Channel::One}})));
            
            passData.ColorOut = graph.Create("ColorOut"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d,
                .Reference = csmTexture,
                .Format = TextureArrayToSliceBindGroupRG::GetColorAttachmentFormat()
            });

            passData.ShadowMap = passData.BindGroup.SetResourcesTextureArray(csmTexture);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut, {
                .OnLoad = AttachmentLoad::Clear,
                .ClearColor = {.F = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}}
            });
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("CSM.Visualize")
            GPU_PROFILE_FRAME("CSM.Visualize")

            auto& cascadeIndex = graph.GetOrCreateBlackboardValue<CascadeIndex>();
            ImGui::Begin("CSM Visualize");
            ImGui::DragInt("CSM cascade", (i32*)&cascadeIndex.Index, 1e-1f, 0, SHADOW_CASCADES);
            ImGui::End();
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
            	.Data = {cascadeIndex.Index}});
            cmd.Draw({.VertexCount = 3});
        });
}
