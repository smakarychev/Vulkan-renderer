#include "MeshletCullMultiviewPass.h"

#include "CullMultiviewResource.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

MeshletCullMultiviewPass::MeshletCullMultiviewPass(RG::Graph& renderGraph, std::string_view name,
    const MeshletCullMultiviewPassInitInfo& info)
        : m_Name(name), m_MultiviewData(info.MultiviewData), m_Stage(info.Stage),
    m_SubsequentTriangleCulling(info.SubsequentTriangleCulling)
{
    ShaderPipelineTemplate* cullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/multiview/meshlet-cull-comp.shader"},
        "Pass.Cull.Multiview.Meshlet", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(cullTemplate)
        .AddSpecialization("REOCCLUSION", m_Stage == CullStage::Reocclusion)
        .AddSpecialization("SINGLE_PASS", m_Stage == CullStage::Single)
        .AddSpecialization("TRIANGLE_CULL", info.SubsequentTriangleCulling)
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

void MeshletCullMultiviewPass::AddToGraph(RG::Graph& renderGraph, MeshletCullMultiviewPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            RgUtils::readWriteCullMeshletMultiview(*info.MultiviewResource, m_Stage, m_SubsequentTriangleCulling,
                graph);
            
            passData.MultiviewResource = info.MultiviewResource;
            
            passData.PipelineData = &m_PipelineData;
            passData.MultiviewData = m_MultiviewData;
            passData.CullStage = m_Stage;
            passData.SubsequentTriangleCulling = m_SubsequentTriangleCulling;

            graph.GetBlackboard().Update(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Meshlet Cull Multiview")
            GPU_PROFILE_FRAME("Meshlet Cull Multiview")

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

            RgUtils::updateMeshletCullMultiviewBindings(resourceDescriptors, resources, *multiview,
                passData.CullStage, passData.SubsequentTriangleCulling, *frameContext.ResourceUploader);

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

            for (u32 i = 0; i < info.MultiviewResource->Objects.size(); i++)
            {
                u32 meshletCount = multiview->ViewDescriptions->at(i).Static.Geometry->GetMeshletCount();
                
                PushConstant pushConstant = {
                    .MeshletCount = meshletCount,
                    .GeometryIndex = i,
                    .ViewCount = (u32)info.MultiviewResource->Views.size()};

                RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstant);

                RenderCommand::Dispatch(cmd,
                    {meshletCount, 1, 1},
                    {64, 1, 1});
            }
        });
}
