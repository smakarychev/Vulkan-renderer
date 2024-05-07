#include "MeshletCullMultiviewPass.h"

#include "CullMultiviewResource.h"
#include "RenderGraph/RenderGraph.h"

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

            graph.GetBlackboard().Update(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Meshlet Cull Multiview")
            GPU_PROFILE_FRAME("Meshlet Cull Multiview")
        });
}
