#include "CSMVisualizePass.h"

#include "FrameContext.h"
#include "imgui/imgui.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/CsmVisualizeBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::VisualizeCSM::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const CSM::PassData& csmOutput, RG::Resource colorIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct CascadeIndex
    {
        u32 Index{0};
    };
    
    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("CSM.Visualize.Setup")

            graph.SetShader("csm-visualize.shader");
            
            const TextureDescription& csmDescription = Resources(graph).GetTextureDescription(csmOutput.ShadowMap);
            
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, std::format("{}.ColorOut", name),
                GraphTextureDescription{
                    .Width = csmDescription.Width,
                    .Height = csmDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.ShadowMap = graph.Read(csmOutput.ShadowMap, Pixel | Sampled);
            passData.CSM = graph.Read(csmOutput.CSM, Pixel | Uniform);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                colorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                AttachmentStore::Store, glm::vec4{0.0f, 0.0f, 0.0f, 1.0f});

            graph.UpdateBlackboard(passData);
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
                    ImageLayout::DepthReadonly : ImageLayout::DepthReadonly);
            bindGroup.SetCsmData({.Buffer = csmData});

            auto& cascadeIndex = resources.GetOrCreateValue<CascadeIndex>();
            ImGui::Begin("CSM Visualize");
            ImGui::DragInt("CSM cascade", (i32*)&cascadeIndex.Index, 1e-1f, 0, SHADOW_CASCADES);
            ImGui::End();
            
            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            RenderCommand::PushConstants(cmd, shader.GetLayout(), cascadeIndex.Index);
            RenderCommand::Draw(cmd, 3);
        });

    return pass;
}
