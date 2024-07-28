#include "CSMVisualizePass.h"

#include "FrameContext.h"
#include "imgui/imgui.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/ShaderCache.h"
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

            graph.SetShader("../assets/shaders/csm-visualize.shader");
            
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

            const Texture& shadowMap = resources.GetTexture(passData.ShadowMap);
            const Buffer& csmData = resources.GetBuffer(passData.CSM);

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_shadow_map", shadowMap.BindingInfo(
                ImageFilter::Linear,
                shadowMap.Description().Format == Format::D32_FLOAT ?
                    ImageLayout::DepthReadonly : ImageLayout::DepthReadonly));
            resourceDescriptors.UpdateBinding("u_csm_data", csmData.BindingInfo());

            auto& cascadeIndex = resources.GetOrCreateValue<CascadeIndex>();
            ImGui::Begin("CSM Visualize");
            ImGui::DragInt("CSM cascade", (i32*)&cascadeIndex.Index, 1e-1f, 0, SHADOW_CASCADES);
            ImGui::End();
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), cascadeIndex.Index);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            
            RenderCommand::Draw(cmd, 3);
        });

    return pass;
}
