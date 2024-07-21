#include "CSMVisualizePass.h"

#include "FrameContext.h"
#include "imgui/imgui.h"
#include "RenderGraph/RGUtils.h"
#include "Vulkan/RenderCommand.h"

CSMVisualizePass::CSMVisualizePass(RG::Graph& renderGraph)
{
    ShaderPipelineTemplate* visualizeTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
       "../assets/shaders/processed/render-graph/common/fullscreen-vert.stage",
       "../assets/shaders/processed/render-graph/shadows/visualize-csm-frag.stage"},
       "Pass.CSM.Visualize", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(visualizeTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT}})
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(visualizeTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(visualizeTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void CSMVisualizePass::AddToGraph(RG::Graph& renderGraph, const CSMPass::PassData& csmOutput, RG::Resource colorIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{"Visualize.CSM"},
        [&](Graph& graph, PassData& passData)
        {
            const TextureDescription& csmDescription = Resources(graph).GetTextureDescription(csmOutput.ShadowMap);
            
            passData.ColorOut = RgUtils::ensureResource(colorIn, graph, "Visualize.CSM.ColorOut",
                GraphTextureDescription{
                    .Width = csmDescription.Width,
                    .Height = csmDescription.Height,
                    .Format = Format::RGBA16_FLOAT});

            passData.ShadowMap = graph.Read(csmOutput.ShadowMap, Pixel | Sampled);
            passData.CSM = graph.Read(csmOutput.CSM, Pixel | Uniform);
            passData.ColorOut = graph.RenderTarget(passData.ColorOut,
                colorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                AttachmentStore::Store, glm::vec4{0.0f, 0.0f, 0.0f, 1.0f});

            passData.PipelineData = &m_PipelineData;
            passData.CascadeIndex = &m_CascadeIndex;

            graph.GetBlackboard().Update(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Visualize CSM")

            const Texture& shadowMap = resources.GetTexture(passData.ShadowMap);
            const Buffer& csmData = resources.GetBuffer(passData.CSM);

            auto& pipeline = passData.PipelineData->Pipeline;    
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;    
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding("u_shadow_map", shadowMap.BindingInfo(
                ImageFilter::Linear,
                shadowMap.Description().Format == Format::D32_FLOAT ?
                    ImageLayout::DepthReadonly : ImageLayout::DepthReadonly));
            resourceDescriptors.UpdateBinding("u_csm_data", csmData.BindingInfo());

            ImGui::Begin("CSM Visualize");
            ImGui::DragInt("CSM cascade", (i32*)passData.CascadeIndex, 1e-1f, 0, SHADOW_CASCADES);
            ImGui::End();
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindGraphicsImmutableSamplers(cmd, pipeline.GetLayout());
            pipeline.BindGraphics(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), *passData.CascadeIndex);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            
            RenderCommand::Draw(cmd, 3);
        });
}
