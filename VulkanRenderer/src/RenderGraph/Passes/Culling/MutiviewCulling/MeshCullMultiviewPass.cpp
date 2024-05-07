#include "MeshCullMultiviewPass.h"

#include "FrameContext.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

MeshCullMultiviewPass::MeshCullMultiviewPass(RG::Graph& renderGraph, std::string_view name,
    const MeshCullMultiviewPassInitInfo& info)
        : m_Name(name), m_MultiviewData(info.MultiviewData), m_Stage(info.Stage)
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

void MeshCullMultiviewPass::AddToGraph(RG::Graph& renderGraph, MeshCullMultiviewPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            if (m_Stage != CullStage::Reocclusion)
                *info.MultiviewResource = RgUtils::createCullMultiview(*m_MultiviewData, graph, m_Name.Name());
            else
                for (u32 i = 0; i < info.MultiviewResource->Views.size(); i++)
                    info.MultiviewResource->HiZs[i] = m_MultiviewData->Views()[i].Static.HiZContext->GetHiZResource();

            RgUtils::readWriteCullMeshMultiview(*info.MultiviewResource, graph);
            
            passData.MultiviewResource = info.MultiviewResource;
            
            passData.PipelineData = &m_PipelineData;
            passData.MultiviewData = m_MultiviewData;

            graph.GetBlackboard().Update(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Mesh Cull Multiview")
            GPU_PROFILE_FRAME("Mesh Cull Multiview")

            auto* multiview = passData.MultiviewResource;

            Sampler hizSampler = multiview->HiZSampler;
            std::vector<CullViewDataGPU> views = passData.MultiviewData->CreateMultiviewGPU();
            for (u32 i = 0; i < views.size(); i++)
                resources.GetBuffer(multiview->Views[i], views[i], *frameContext.ResourceUploader);
            
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& samplerDescriptors = passData.PipelineData->SamplerDescriptors;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

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

            for (u32 i = 0; i < info.MultiviewResource->Objects.size(); i++)
            {
                u32 meshCount = multiview->ViewDescriptions->at(i).Static.Geometry->GetRenderObjectCount();
                
                PushConstant pushConstant = {
                    .ObjectCount = meshCount,
                    .GeometryIndex = i,
                    .ViewCount = (u32)info.MultiviewResource->Views.size()};

                RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstant);

                RenderCommand::Dispatch(cmd,
                    {meshCount, 1, 1},
                    {64, 1, 1});
            }
        });
}
