#include "TriangleCullDrawPass.h"

#include "MeshletCullPass.h"

TriangleCullContext::TriangleCullContext(MeshletCullContext& meshletCullContext)
    : m_MeshletCullContext(&meshletCullContext)
{
    m_Visibility = Buffer::Builder({
            .SizeBytes = (u64)(MAX_TRIANGLES * SUB_BATCH_COUNT *
                (u32)sizeof(SceneGeometry::TriangleVisibilityType)),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();
}


TriangleCullPrepareDispatchPass::TriangleCullPrepareDispatchPass(
    RG::Graph& renderGraph, std::string_view name)
        : m_Name(name)
{
    ShaderPipelineTemplate* prepareDispatchTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/prepare-indirect-dispatches-comp.shader"},
        "Pass.TriangleCull.PrepareDispatch", renderGraph.GetArenaAllocators());

    m_PipelineData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(prepareDispatchTemplate)
        .UseDescriptorBuffer()
        .Build();

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(prepareDispatchTemplate, DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();
}

void TriangleCullPrepareDispatchPass::AddToGraph(RG::Graph& renderGraph,
    TriangleCullContext& ctx)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    std::string name = m_Name.Name();
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            passData.MaxDispatches = ctx.Geometry().GetCommandCount() / TriangleCullContext::GetCommandCount() + 1;
            auto& meshletResources = ctx.MeshletContext().Resources();
            ctx.Resources().Dispatch = graph.CreateResource(std::format("{}.{}", name, "Dispatch"),
                GraphBufferDescription{.SizeBytes = RenderUtils::alignUniformBufferSizeBytes(
                    passData.MaxDispatches * sizeof(IndirectDispatchCommand))});
            ctx.Resources().Dispatch = graph.Write(ctx.Resources().Dispatch, Compute | Storage | Upload);

            ctx.MeshletContext().Resources().CompactCount =
                    graph.Read(ctx.MeshletContext().Resources().CompactCount, Readback);
                passData.CompactCount = graph.Read(meshletResources.CompactCount, Compute | Storage);
            
            passData.Dispatch = ctx.Resources().Dispatch;
            passData.PipelineData = &m_PipelineData;
            passData.Context = &ctx;

            graph.GetBlackboard().Register(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Prepare Dispatch Indirect")

            const Buffer& count = resources.GetBuffer(passData.CompactCount);
            const Buffer& dispatch = resources.GetBuffer(passData.Dispatch);
            
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding("u_command_count", count.BindingInfo());          
            resourceDescriptors.UpdateBinding("u_indirect_dispatch", dispatch.BindingInfo());

            struct PushConstants
            {
                u32 CommandsPerBatchCount;
                u32 CommandsMultiplier;
                u32 LocalGroupX;
                u32 MaxDispatchesCount;
            };
            PushConstants pushConstants = {
                .CommandsPerBatchCount = TriangleCullContext::GetCommandCount(),
                .CommandsMultiplier = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .LocalGroupX = assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
                .MaxDispatchesCount = passData.MaxDispatches};
            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            RenderCommand::Dispatch(cmd, {pushConstants.MaxDispatchesCount, 1, 1}, {64, 1, 1});

            RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
                .MemoryDependency({
                    .SourceStage = PipelineStage::ComputeShader,
                    .DestinationStage = PipelineStage::Host,
                    .SourceAccess = PipelineAccess::WriteShader,
                    .DestinationAccess = PipelineAccess::ReadHost})
                .Build(frameContext.DeletionQueue));

            // todo: add some bias (like multiply by 1.25 for example)?
            u32 visibleMeshletsValue = passData.Context->MeshletContext().ReadbackCompactCountValue();
            
            u32 commandCount = TriangleCullContext::GetCommandCount();
            u32 iterationCount = visibleMeshletsValue / commandCount + (u32)(visibleMeshletsValue % commandCount != 0);
            passData.Context->SetIterationCount(iterationCount);
        });
}