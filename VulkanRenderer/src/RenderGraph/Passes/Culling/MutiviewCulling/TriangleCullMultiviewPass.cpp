#include "TriangleCullMultiviewPass.h"

#include "FrameContext.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

TriangleCullPrepareMultiviewPass::TriangleCullPrepareMultiviewPass(RG::Graph& renderGraph, std::string_view name)
    : m_Name(name)
{
    ShaderPipelineTemplate* prepareDispatchTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/multiview/prepare-dispatches-comp.shader"},
        "Pass.TriangleCull.Multiview.PrepareDispatch", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(prepareDispatchTemplate)
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(prepareDispatchTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void TriangleCullPrepareMultiviewPass::AddToGraph(RG::Graph& renderGraph,
    RG::CullTrianglesMultiviewResource* multiviewResource)
{
    using namespace RG;

    m_Pass = &renderGraph.AddRenderPass<PassData>(m_Name,
        [&](Graph& graph, PassData& passData)
        {
            RgUtils::readWriteCullTrianglePrepareMultiview(*multiviewResource, renderGraph);
            
            passData.MultiviewResource = multiviewResource;
            
            passData.PipelineData = &m_PipelineData;
            
            graph.GetBlackboard().Register(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Triangle Cull Prepare Multiview")
            GPU_PROFILE_FRAME("Triangle Cull Prepare Multiview")
            
            auto* multiview = passData.MultiviewResource;

            u32 maxDispatchesTotal = 0;
            for (u32 i = 0; i < multiview->BatchDispatches.size(); i++)
            {
                u32 meshletViewIndex = multiview->MeshletViewIndices[i];
                auto* geometry = multiview->CullResources->Multiview->Views()[meshletViewIndex].Static.Geometry;
                u32 maxDispatches = TriangleCullMultiviewTraits::MaxDispatches(geometry->GetCommandCount());
                resources.GetBuffer(multiview->MaxDispatches, maxDispatches, i * sizeof(u32),
                    *frameContext.ResourceUploader);

                maxDispatchesTotal = std::max(maxDispatchesTotal, maxDispatches);
            }
            
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            RgUtils::updateCullTrianglePrepareMultiviewBindings(resourceDescriptors, resources, *multiview);
            struct PushConstants
            {
                u32 CommandsPerBatchCount;
                u32 CommandsMultiplier;
                u32 LocalGroupX;
                u32 ViewCount;
            };

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            PushConstants pushConstants = {
                .CommandsPerBatchCount = TriangleCullMultiviewTraits::CommandCount(),
                .CommandsMultiplier = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .LocalGroupX = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .ViewCount = (u32)multiview->BatchDispatches.size()};
            
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
            
            RenderCommand::Dispatch(cmd,
                {maxDispatchesTotal, 1, 1},
                {64, 1, 1});

            // todo: readback
        });
}
