#include "MeshCullMultiviewPass.h"

#include "FrameContext.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

MeshCullMultiviewPass::MeshCullMultiviewPass(RG::Graph& renderGraph, std::string_view name,
    const MeshCullMultiviewPassInitInfo& info)
        : m_Name(name), m_Stage(info.Stage)
{
    ShaderPipelineTemplate* cullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/multiview/mesh-cull-comp.shader"},
        "Pass.Cull.Multiview.Mesh", renderGraph.GetArenaAllocators());

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

void MeshCullMultiviewPass::AddToGraph(RG::Graph& renderGraph, const MeshCullMultiviewPassExecutionInfo& info)
{
    using namespace RG;
    
    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            if (m_Stage != CullStage::Cull)
                for (u32 i = 0; i < info.MultiviewResource->ViewCount; i++)
                    info.MultiviewResource->HiZs[i] =
                        info.MultiviewResource->Multiview->Views()[i].Static.HiZContext->GetHiZResource();

            RgUtils::readWriteCullMeshMultiview(*info.MultiviewResource, graph);
            
            passData.MultiviewResource = info.MultiviewResource;
            
            passData.PipelineData = &m_PipelineData;

            graph.GetBlackboard().Update(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Mesh Cull Multiview")
            GPU_PROFILE_FRAME("Mesh Cull Multiview")

            auto* multiview = passData.MultiviewResource;
            auto* multiviewData = multiview->Multiview;

            resources.GetBuffer(multiview->ViewSpans, multiviewData->ViewSpans().data(),
                multiviewData->ViewSpans().size() * sizeof(CullMultiviewData::ViewSpan), 0,
                *frameContext.ResourceUploader);
            std::vector<CullViewDataGPU> views = multiviewData->CreateMultiviewGPU();
            resources.GetBuffer(multiview->Views, views.data(), views.size() * sizeof(CullViewDataGPU), 0,
                *frameContext.ResourceUploader);
            
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            Sampler hizSampler = multiview->HiZSampler;
            samplerDescriptors.UpdateBinding("u_sampler", resources.GetTexture(
                multiview->HiZs.front()).BindingInfo(hizSampler, ImageLayout::DepthReadonly));

            RgUtils::updateMeshCullMultiviewBindings(resourceDescriptors, resources, *multiview);

            struct PushConstant
            {
                u32 ObjectCount;
                u32 GeometryIndex;
                u32 ViewCount;
            };
                       
            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            samplerDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            for (u32 i = 0; i < info.MultiviewResource->GeometryCount; i++)
            {
                u32 meshCount = multiview->Multiview->Views()[i].Static.Geometry->GetRenderObjectCount();
                
                PushConstant pushConstant = {
                    .ObjectCount = meshCount,
                    .GeometryIndex = i,
                    .ViewCount = info.MultiviewResource->ViewCount};

                RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstant);

                RenderCommand::Dispatch(cmd,
                    {meshCount, 1, 1},
                    {64, 1, 1});
            }
        });
}
