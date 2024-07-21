#include "MeshletCullMultiviewPass.h"

#include "CullMultiviewResources.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

MeshletCullMultiviewPass::MeshletCullMultiviewPass(RG::Graph& renderGraph, std::string_view name,
    const MeshletCullMultiviewPassInitInfo& info)
        : m_Name(name), m_Stage(info.Stage)
{
    ShaderPipelineTemplate* cullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/multiview/meshlet-cull-comp.stage"},
        "Pass.Cull.Multiview.Meshlet", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(cullTemplate)
        .AddSpecialization("REOCCLUSION", m_Stage == CullStage::Reocclusion)
        .AddSpecialization("SINGLE_PASS", m_Stage == CullStage::Single)
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.SamplerDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(cullTemplate, DescriptorAllocatorKind::Samplers)
        .ExtractSet(0)
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(cullTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void MeshletCullMultiviewPass::AddToGraph(RG::Graph& renderGraph, const MeshletCullMultiviewPassExecutionInfo& info)
{
    using namespace RG;

    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            RgUtils::readWriteCullMeshletMultiview(*info.MultiviewResource, m_Stage, graph);
            
            passData.MultiviewResource = info.MultiviewResource;
            
            passData.PipelineData = &m_PipelineData;
            passData.CullStage = m_Stage;

            graph.GetBlackboard().Update(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Meshlet Cull Multiview")
            GPU_PROFILE_FRAME("Meshlet Cull Multiview")

            auto* multiview = passData.MultiviewResource;

            if (passData.CullStage == CullStage::Reocclusion)
                for (u32 i = 0; i < multiview->ViewCount; i++)
                    resources.GetBuffer(multiview->CompactCommandCountReocclusion, 0u, i * sizeof(u32),
                        *frameContext.ResourceUploader);
            else
                for (u32 i = 0; i < multiview->ViewCount + multiview->GeometryCount; i++)
                    resources.GetBuffer(multiview->CompactCommandCount, 0u, i * sizeof(u32),
                        *frameContext.ResourceUploader);

            Sampler hizSampler = multiview->HiZSampler;
            
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            samplerDescriptors.UpdateBinding("u_sampler", resources.GetTexture(
                multiview->HiZs.front()).BindingInfo(hizSampler, ImageLayout::DepthReadonly));

            RgUtils::updateCullMeshletMultiviewBindings(resourceDescriptors, resources, *multiview, passData.CullStage);

            struct PushConstant
            {
                u32 MeshletCount;
                u32 GeometryIndex;
                u32 ViewCount;
            };
                       
            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            for (u32 i = 0; i < info.MultiviewResource->GeometryCount; i++)
            {
                u32 meshletCount = multiview->Multiview->View(i).Static.Geometry->GetMeshletCount();
                
                PushConstant pushConstant = {
                    .MeshletCount = meshletCount,
                    .GeometryIndex = i,
                    .ViewCount = info.MultiviewResource->ViewCount};

                RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstant);

                RenderCommand::Dispatch(cmd,
                    {meshletCount, 1, 1},
                    {64, 1, 1});
            }
        });
}
