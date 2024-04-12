#include "TriangleCullDrawPass.h"

#include "MeshletCullPass.h"

TriangleCullContext::TriangleCullContext(MeshletCullContext& meshletCullContext)
    : m_MeshletCullContext(&meshletCullContext)
{
    m_Visibility = Buffer::Builder({
            .SizeBytes = (u64)(MAX_TRIANGLES * SUB_BATCH_COUNT *
                (u32)sizeof(RG::Geometry::TriangleVisibilityType)),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();
}


TriangleCullPrepareDispatchPass::TriangleCullPrepareDispatchPass(
    RG::Graph& renderGraph, std::string_view name)
        : m_Name(name)
{
    ShaderPipelineTemplate* prepareDispatchTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/culling/prepare-indirect-dispatches-comp.shader"},
        "render-graph-prepare-triangle-cull-pass-template", renderGraph.GetArenaAllocators());

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

    static ShaderDescriptors::BindingInfo countBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_command_count"); 
    static ShaderDescriptors::BindingInfo dispatchBinding =
        m_PipelineData.ResourceDescriptors.GetBindingInfo("u_indirect_dispatch"); 

    std::string name = m_Name.Name();
    m_Pass = &renderGraph.AddRenderPass<PassData>(PassName{name},
        [&](Graph& graph, PassData& passData)
        {
            passData.MaxDispatches = ctx.Geometry().GetCommandCount() / TriangleCullContext::GetCommandCount() + 1;
            auto& meshletResources = ctx.MeshletContext().Resources();
            ctx.Resources().DispatchIndirect = graph.CreateResource(std::format("{}.{}", name, "Dispatch"),
                GraphBufferDescription{.SizeBytes = renderUtils::alignUniformBufferSizeBytes(
                    passData.MaxDispatches * sizeof(IndirectDispatchCommand))});
            ctx.Resources().DispatchIndirect = graph.Write(ctx.Resources().DispatchIndirect, Compute | Storage | Upload);

            ctx.MeshletContext().Resources().CompactCountSsbo =
                    graph.Read(ctx.MeshletContext().Resources().CompactCountSsbo, Readback);
                passData.CompactCountSsbo = graph.Read(meshletResources.CompactCountSsbo, Compute | Storage);
            
            passData.DispatchIndirect = ctx.Resources().DispatchIndirect;
            passData.PipelineData = &m_PipelineData;
            passData.Context = &ctx;

            graph.GetBlackboard().Register(m_Name.Hash(), passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Prepare Dispatch Indirect")

            const Buffer& countSsbo = resources.GetBuffer(passData.CompactCountSsbo);
            const Buffer& dispatchSsbo = resources.GetBuffer(passData.DispatchIndirect);
            
            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding(countBinding, countSsbo.BindingInfo());          
            resourceDescriptors.UpdateBinding(dispatchBinding, dispatchSsbo.BindingInfo());

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

            // todo: fix me! this is bad! please!
            // readback cull iteration count
            //resources.GetGraph()->OnCmdEnd(frameContext);
            //cmd.End();
            //Fence readbackFence = Fence::Builder().BuildManualLifetime();
            //cmd.Submit(Driver::GetDevice().GetQueues().Graphics, readbackFence);
            //readbackFence.Wait();
            //Fence::Destroy(readbackFence);
            //cmd.Begin();
            //resources.GetGraph()->OnCmdBegin(frameContext);

            RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
                .MemoryDependency({
                    .SourceStage = PipelineStage::ComputeShader,
                    .DestinationStage = PipelineStage::Host,
                    .SourceAccess = PipelineAccess::WriteShader,
                    .DestinationAccess = PipelineAccess::ReadHost})
                .Build(frameContext.DeletionQueue));
            
            u32 visibleMeshletsValue = 0;
            visibleMeshletsValue = passData.Context->MeshletContext().CompactCountValue();
            
            u32 commandCount = TriangleCullContext::GetCommandCount();
            u32 iterationCount = visibleMeshletsValue / commandCount + (u32)(visibleMeshletsValue % commandCount != 0);
            passData.Context->SetIterationCount(iterationCount);
            passData.Context->ResetIteration();
        });
}