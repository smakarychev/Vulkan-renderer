#include "RenderPassGeometryCull.h"

#include <glm/glm.hpp>

#include "Renderer.h"
#include "RenderPassGeometry.h"
#include "Core/Camera.h"
#include "Rendering/Buffer.h"
#include "Rendering/DepthPyramid.h"
#include "Rendering/RenderingUtils.h"
#include "Vulkan/RenderCommand.h"

struct CameraCullData
{
    glm::mat4 ViewMatrix;
    FrustumPlanes FrustumPlanes;
    ProjectionData ProjectionData;
    f32 PyramidWidth;
    f32 PyramidHeight;
    
    u64 Padding;
};

struct RenderPassGeometryCull::CullBuffers
{
    void Init(const RenderPassGeometry& renderPassGeometry);
    void Shutdown();
    
    struct CameraCullDataUBO
    {
        CameraCullData CameraData;
        Buffer Buffer;
    };
    struct CountBuffers
    {
        Buffer VisibleMeshlets;
    };
    struct VisibilityBuffers
    {
        Buffer MeshVisibility;
        Buffer MeshletVisibility;
        Buffer TriangleVisibility;
    };

    CameraCullDataUBO CameraDataUBO{};
    CameraCullDataUBO CameraDataUBOExtended{};
    CountBuffers CountBuffers{};
    
    VisibilityBuffers VisibilityBuffers;
    void* VisibleMeshletCountBufferMappedAddress{nullptr};

    Buffer CompactedCommands{};
    Buffer Triangles{};
    Buffer BatchIndirectDispatches{};
    Buffer BatchCompactIndirectDispatches{};
};

struct CullBatch
{
    static constexpr u32 MAX_TRIANGLES = 128'000;
    static constexpr u32 MAX_INDICES = MAX_TRIANGLES * 3;
    static constexpr u32 MAX_COMMANDS = MAX_TRIANGLES / assetLib::ModelInfo::TRIANGLES_PER_MESHLET;

    CullBatch();
    ~CullBatch() = default;
    CullBatch(const CullBatch&) = delete;
    CullBatch(const CullBatch&&) = delete;
    CullBatch& operator=(const CullBatch&) = delete;
    CullBatch& operator=(CullBatch&&) = delete;

    static u32 GetCommandCount()
    {
        return MAX_COMMANDS * SUB_BATCH_COUNT;
    }
    static u64 GetCommandsSizeBytes()
    {
        return renderUtils::alignUniformBufferSizeBytes(MAX_COMMANDS * sizeof(IndirectCommand) * SUB_BATCH_COUNT);
    }
    static u64 GetTrianglesSizeBytes()
    {
        return renderUtils::alignUniformBufferSizeBytes(MAX_TRIANGLES * sizeof(u8) * SUB_BATCH_COUNT);
    }

    Buffer Indices;
    Buffer Count;
    Buffer Draw;
};

class RenderPassGeometryCull::BatchedCull
{
    struct ComputePipelineData;
public:
    void Init(const RenderPassGeometry& renderPassGeometry, const CullBuffers& cullBuffers,
        DescriptorAllocator& persistentAllocator, DescriptorAllocator& resolutionDependentAllocator);
    void Shutdown(bool shouldFree);

    void SetDepthPyramid(const DepthPyramid& depthPyramid,
        const RenderPassGeometry& renderPassGeometry, const CullBuffers& cullBuffers,
        const glm::uvec2& renderResolution, bool shouldFree);
    
    void BatchIndirectDispatchesBuffersPrepare(const CullContextExtended& cullContext);
    void CullTriangles(const CullContextExtended& cullContext);
    const CullBatch& GetCullDrawBatch() const { return *m_CullBatches[m_CurrentBatch]; }
    void NextSubBatch();
    void ResetSubBatches();

    u64 GetTrianglesOffset() const { return CullBatch::GetTrianglesSizeBytes() * m_CurrentBatch; }

    u32 GetBatchIndex() const { return m_CurrentBatch; }
    
    u32 ReadBackBatchCount(const CullBuffers& cullBuffers, u32 frameNumber) const;
private:
    void InitPipelines(const CullBuffers& cullBuffers,
        DescriptorAllocator& persistentAllocator, DescriptorAllocator& resolutionDependentAllocator);
    void FreeResolutionDependentResources() const;
    void RecordCommandBuffers(const CullBuffers& cullBuffers, const glm::uvec2& resolution);
private:
    std::array<std::unique_ptr<CullBatch>, CULL_BATCH_OVERLAP> m_CullBatches;

    struct ComputeBatchData
    {
        RenderGraph::PipelineData TriangleCullSingular{};
        RenderGraph::PipelineData TriangleCullReocclusionSingular{};
        RenderGraph::PipelineData PrepareDrawSingular{};
    };
    std::array<ComputeBatchData, CULL_BATCH_OVERLAP> m_CullDrawBatchData{};
    RenderGraph::PipelineData m_PrepareIndirectDispatches{};
    RenderGraph::PipelineData m_PrepareCompactIndirectDispatches{};
    
    u32 m_CurrentBatch{0};
    u32 m_CurrentBatchFlat{0};
    u32 m_MaxBatchDispatches{0};

    CommandPool m_CommandPool;
    std::array<std::vector<CommandBuffer>, BUFFERED_FRAMES> m_TriangleCullCmds;
    std::array<std::vector<CommandBuffer>, BUFFERED_FRAMES> m_TriangleReocclusionCullCmds;
};


CullBatch::CullBatch()
{
    Indices = Buffer::Builder()
        .SetUsage(BufferUsage::Index | BufferUsage::Storage)
        .SetSizeBytes(MAX_INDICES * sizeof(u32) * SUB_BATCH_COUNT)
        .Build();

    Count = Buffer::Builder()
        .SetUsage(BufferUsage::Indirect | BufferUsage::Storage)
        .SetSizeBytes(sizeof(u32))
        .Build();

    Draw = Buffer::Builder()
        .SetUsage(BufferUsage::Indirect | BufferUsage::Storage | BufferUsage::Destination)
        .SetSizeBytes(sizeof(IndirectCommand))
        .Build();
}

void RenderPassGeometryCull::BatchedCull::Init(const RenderPassGeometry& renderPassGeometry,
    const CullBuffers& cullBuffers,
    DescriptorAllocator& persistentAllocator, DescriptorAllocator& resolutionDependentAllocator)
{
    for (auto& batch : m_CullBatches)
        batch = std::make_unique<CullBatch>();

    m_MaxBatchDispatches = renderPassGeometry.GetCommandCount() / CullBatch::GetCommandCount() + 1;

    InitPipelines(cullBuffers, persistentAllocator, resolutionDependentAllocator);
}

void RenderPassGeometryCull::BatchedCull::Shutdown(bool shouldFree)
{
    if (shouldFree)
        FreeResolutionDependentResources();

    for (auto& batch : m_CullBatches)
        batch.reset();
}

void RenderPassGeometryCull::BatchedCull::SetDepthPyramid(const DepthPyramid& depthPyramid,
    const RenderPassGeometry& renderPassGeometry, const CullBuffers& cullBuffers,
    const glm::uvec2& renderResolution, bool shouldFree)
{
    if (shouldFree)
        FreeResolutionDependentResources();

    for (u32 i = 0; i < CULL_BATCH_OVERLAP; i++)
    {
        auto& batchData = m_CullDrawBatchData[i];

        batchData.TriangleCullSingular.Descriptors = ShaderDescriptorSet::Builder()
            .SetTemplate(batchData.TriangleCullSingular.Pipeline.GetTemplate())
            .AddBinding("u_scene_data", cullBuffers.CameraDataUBOExtended.Buffer, sizeof(CameraCullData), 0)
            .AddBinding("u_depth_pyramid",  depthPyramid.GetTexture().CreateBindingInfo(
                depthPyramid.GetSampler(), ImageLayout::General))
            .AddBinding("u_object_buffer", renderPassGeometry.GetRenderObjectsBuffer())
            .AddBinding("u_meshlet_visibility_buffer", cullBuffers.VisibilityBuffers.MeshletVisibility)
            .AddBinding("u_positions_buffer", renderPassGeometry.GetAttributeBuffers().Positions)
            .AddBinding("u_indices_buffer", renderPassGeometry.GetAttributeBuffers().Indices)
            .AddBinding("u_singular_indices_buffer", m_CullBatches[i]->Indices)
            .AddBinding("u_singular_index_count_buffer", m_CullBatches[i]->Count)
            .AddBinding("u_visible_triangles_buffer", cullBuffers.VisibilityBuffers.TriangleVisibility)
            .AddBinding("u_triangle_buffer", cullBuffers.Triangles, CullBatch::GetTrianglesSizeBytes(), 0)
            .AddBinding("u_command_buffer", cullBuffers.CompactedCommands)
            .AddBinding("u_count_buffer", cullBuffers.CountBuffers.VisibleMeshlets, sizeof(u32), 0)
            .Build();

        batchData.TriangleCullReocclusionSingular.Descriptors = batchData.TriangleCullSingular.Descriptors;
    }

    m_CommandPool = CommandPool::Builder()
        .SetQueue(QueueKind::Graphics)
        .BuildManualLifetime();

    RecordCommandBuffers(cullBuffers, renderResolution);
}

void RenderPassGeometryCull::BatchedCull::BatchIndirectDispatchesBuffersPrepare(const CullContextExtended& cullContext)
{
    const CommandBuffer& cmd = *cullContext.Cmd;
    
    u32 countOffset = (u32)renderUtils::alignUniformBufferSizeBytes(sizeof(u32)) * cullContext.FrameNumber;
    u32 dispatchIndirectOffset = (u32)renderUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(
        m_MaxBatchDispatches) * cullContext.FrameNumber;

    std::vector<u32> pushConstants = {
        CullBatch::GetCommandCount(),
        assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
        assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
        m_MaxBatchDispatches};
    
    m_PrepareIndirectDispatches.Pipeline.BindCompute(cmd);
    m_PrepareIndirectDispatches.Descriptors.BindCompute(cmd, DescriptorKind::Global,
        m_PrepareIndirectDispatches.Pipeline.GetLayout(), {countOffset, dispatchIndirectOffset});
    RenderCommand::PushConstants(cmd, m_PrepareIndirectDispatches.Pipeline.GetLayout(),
        pushConstants.data());
    RenderCommand::Dispatch(cmd, {m_MaxBatchDispatches / 64 + 1, 1, 1});

    pushConstants = {
        CullBatch::GetCommandCount(),
        1,
        assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
        m_MaxBatchDispatches};
    m_PrepareCompactIndirectDispatches.Pipeline.BindCompute(cmd);
    m_PrepareCompactIndirectDispatches.Descriptors.BindCompute(cmd, DescriptorKind::Global,
        m_PrepareCompactIndirectDispatches.Pipeline.GetLayout(), {countOffset, dispatchIndirectOffset});
    RenderCommand::PushConstants(cmd, m_PrepareCompactIndirectDispatches.Pipeline.GetLayout(),
        pushConstants.data());
    RenderCommand::Dispatch(cmd, {m_MaxBatchDispatches / 64 + 1, 1, 1});

    DependencyInfo indirectDependency = DependencyInfo::Builder()
        .MemoryDependency({
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage = PipelineStage::Indirect,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadIndirect})
        .Build(*cullContext.DeletionQueue);
    RenderCommand::WaitOnBarrier(cmd, indirectDependency);
}

void RenderPassGeometryCull::BatchedCull::CullTriangles(const CullContextExtended& cullContext)
{
    const CommandBuffer& cmd = *cullContext.Cmd;

    RenderCommand::ExecuteSecondaryCommandBuffer(cmd, cullContext.Reocclusion ?
        m_TriangleReocclusionCullCmds[cullContext.FrameNumber][m_CurrentBatchFlat] :
        m_TriangleCullCmds[cullContext.FrameNumber][m_CurrentBatchFlat]);
}

void RenderPassGeometryCull::BatchedCull::NextSubBatch()
{
    m_CurrentBatchFlat++;
    m_CurrentBatch = (m_CurrentBatchFlat) % CULL_BATCH_OVERLAP;
}

void RenderPassGeometryCull::BatchedCull::ResetSubBatches()
{
    m_CurrentBatchFlat = 0;
    m_CurrentBatch = 0;
}

u32 RenderPassGeometryCull::BatchedCull::ReadBackBatchCount(const CullBuffers& cullBuffers, u32 frameNumber) const
{
    void* address = cullBuffers.VisibleMeshletCountBufferMappedAddress;
    address = (u8*)address + renderUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameNumber;
    u32 visibleMeshletsValue = *(u32*)(address);
    u32 commandCount = CullBatch::GetCommandCount();

    return visibleMeshletsValue / commandCount + (u32)(visibleMeshletsValue % commandCount != 0); 
}

void RenderPassGeometryCull::BatchedCull::InitPipelines(const CullBuffers& cullBuffers,
    DescriptorAllocator& persistentAllocator, DescriptorAllocator& resolutionDependentAllocator)
{
    ShaderPipelineTemplate* triangleCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/triangle-cull-singular-comp.shader"}, "triangle-cull-singular",
        resolutionDependentAllocator);

    ShaderPipelineTemplate* prepareDrawTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/prepare-draw-singular-comp.shader"}, "prepare-draw-singular",
        persistentAllocator);

    ShaderPipelineTemplate* prepareDispatchTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/prepare-indirect-dispatches-comp.shader"}, "prepare-batch-dispatch",
        persistentAllocator);
    
    auto& firstBatchData = m_CullDrawBatchData.front();
    
    firstBatchData.TriangleCullSingular.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(triangleCullTemplate)
        .Build();
    firstBatchData.TriangleCullReocclusionSingular.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(triangleCullTemplate)
        .AddSpecialization("REOCCLUSION", true)
        .Build();

    firstBatchData.PrepareDrawSingular.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(prepareDrawTemplate)
        .Build();

    for (u32 i = 1; i < CULL_BATCH_OVERLAP; i++)
    {
        m_CullDrawBatchData[i].TriangleCullSingular.Pipeline = firstBatchData.TriangleCullSingular.Pipeline;
        m_CullDrawBatchData[i].TriangleCullReocclusionSingular.Pipeline =
            firstBatchData.TriangleCullReocclusionSingular.Pipeline;
        m_CullDrawBatchData[i].PrepareDrawSingular.Pipeline = firstBatchData.PrepareDrawSingular.Pipeline;
    }
    
    for (u32 i = 0; i < CULL_BATCH_OVERLAP; i++)
    {
        auto& batchData = m_CullDrawBatchData[i];

        batchData.PrepareDrawSingular.Descriptors = ShaderDescriptorSet::Builder()
            .SetTemplate(prepareDrawTemplate)
            .AddBinding("u_singular_index_count_buffer", m_CullBatches[i]->Count)
            .AddBinding("u_indirect_draw_buffer", m_CullBatches[i]->Draw)
            .Build();
    }

    m_PrepareIndirectDispatches.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(prepareDispatchTemplate)
        .Build();
    m_PrepareCompactIndirectDispatches.Pipeline = m_PrepareIndirectDispatches.Pipeline;

    m_PrepareIndirectDispatches.Descriptors = ShaderDescriptorSet::Builder()
        .SetTemplate(prepareDispatchTemplate)
        .AddBinding("u_command_count_buffer", cullBuffers.CountBuffers.VisibleMeshlets, sizeof(u32), 0)
        .AddBinding("u_indirect_dispatch_buffer", cullBuffers.BatchIndirectDispatches,
            m_MaxBatchDispatches * sizeof(VkDispatchIndirectCommand), 0)
        .Build();

    m_PrepareCompactIndirectDispatches.Descriptors = ShaderDescriptorSet::Builder()
        .SetTemplate(prepareDispatchTemplate)
        .AddBinding("u_command_count_buffer", cullBuffers.CountBuffers.VisibleMeshlets, sizeof(u32), 0)
        .AddBinding("u_indirect_dispatch_buffer", cullBuffers.BatchCompactIndirectDispatches,
            m_MaxBatchDispatches * sizeof(VkDispatchIndirectCommand), 0)
        .Build();
}

void RenderPassGeometryCull::BatchedCull::FreeResolutionDependentResources() const
{
    CommandPool::Destroy(m_CommandPool);
}

void RenderPassGeometryCull::BatchedCull::RecordCommandBuffers(const CullBuffers& cullBuffers,
    const glm::uvec2& resolution)
{
    auto recordTriangleCullBuffers = [&](bool reocclusion) -> std::array<std::vector<CommandBuffer>, BUFFERED_FRAMES>
    {
        std::array<std::vector<CommandBuffer>, BUFFERED_FRAMES> commandBuffers;

        for (u32 frameIndex = 0; frameIndex < BUFFERED_FRAMES; frameIndex++)
        {
            commandBuffers[frameIndex].reserve(m_MaxBatchDispatches);

            for (u32 dispatchIndex = 0; dispatchIndex < m_MaxBatchDispatches; dispatchIndex++)
            {
                u32 batchIndex = dispatchIndex % CULL_BATCH_OVERLAP;
                
                CommandBuffer cmd = m_CommandPool.AllocateBuffer(CommandBufferKind::Secondary);
                cmd.Begin(CommandBufferUsage::MultipleSubmit | CommandBufferUsage::SimultaneousUse);

                ComputeBatchData& batchData = m_CullDrawBatchData[batchIndex];
                CullBatch& batch = *m_CullBatches[batchIndex];
                u32 commandCount = CullBatch::GetCommandCount();
                
                u64 dispatchIndirectOffset =
                    renderUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(m_MaxBatchDispatches) * frameIndex;
                dispatchIndirectOffset += sizeof(VkDispatchIndirectCommand) * dispatchIndex;
                u32 countOffset = (u32)renderUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameIndex;
                u32 commandOffset = commandCount * dispatchIndex;

                u32 sceneCullOffset =
                    (u32)renderUtils::alignUniformBufferSizeBytes(sizeof(CameraCullData)) * frameIndex;
                u32 triangleOffset = (u32)batch.GetTrianglesSizeBytes() * batchIndex;

                RenderGraph::PipelineData& pipelineDataSingular = reocclusion ?
                    batchData.TriangleCullReocclusionSingular : batchData.TriangleCullSingular;
                
                pipelineDataSingular.Pipeline.BindCompute(cmd);
                pipelineDataSingular.Descriptors.BindCompute(cmd, DescriptorKind::Global,
                    pipelineDataSingular.Pipeline.GetLayout(), {sceneCullOffset, triangleOffset, countOffset});
                std::vector<u32> pushConstantsCull(4);
                pushConstantsCull[0] = resolution.x; 
                pushConstantsCull[1] = resolution.y; 
                pushConstantsCull[2] = commandOffset;   
                pushConstantsCull[3] = commandCount; 
                RenderCommand::PushConstants(cmd, pipelineDataSingular.Pipeline.GetLayout(),
                    pushConstantsCull.data());
                RenderCommand::DispatchIndirect(cmd, cullBuffers.BatchIndirectDispatches, dispatchIndirectOffset);

                DependencyInfo computeDependency = DependencyInfo::Builder()
                    .MemoryDependency({
                        .SourceStage = PipelineStage::ComputeShader,
                        .DestinationStage = PipelineStage::ComputeShader,
                        .SourceAccess = PipelineAccess::WriteShader,
                        .DestinationAccess = PipelineAccess::ReadShader})
                    .Build();
                RenderCommand::WaitOnBarrier(cmd, computeDependency);

                batchData.PrepareDrawSingular.Pipeline.BindCompute(cmd);
                batchData.PrepareDrawSingular.Descriptors.BindCompute(cmd, DescriptorKind::Global,
                    batchData.PrepareDrawSingular.Pipeline.GetLayout());
                RenderCommand::Dispatch(cmd, {1, 1, 1});

                cmd.End();
                commandBuffers[frameIndex].push_back(cmd);
            }
        }

        return commandBuffers;
    };

    m_TriangleCullCmds = recordTriangleCullBuffers(false);
    m_TriangleReocclusionCullCmds = recordTriangleCullBuffers(true);
}


void RenderPassGeometryCull::CullBuffers::Init(const RenderPassGeometry& renderPassGeometry)
{
    Buffer::Builder uboBuilder = Buffer::Builder()
        .SetUsage(BufferUsage::Uniform | BufferUsage::Upload);

    Buffer::Builder ssboReadBackBuilder = Buffer::Builder()
        .SetUsage(BufferUsage::Storage | BufferUsage::Readback);
    
    Buffer::Builder ssboBuilder = Buffer::Builder()
        .SetUsage(BufferUsage::Storage);
    
    Buffer::Builder ssboIndirectBuilder = Buffer::Builder()
        .SetUsage(BufferUsage::Indirect | BufferUsage::Storage | BufferUsage::Destination);
    
    u32 maxBatchDispatches = renderPassGeometry.GetCommandCount() / CullBatch::GetCommandCount() + 1;

    Buffer::Builder indirectDispatchBuilder = Buffer::Builder()
        .SetUsage(BufferUsage::Indirect | BufferUsage::Storage);
    
    CameraDataUBO.Buffer = uboBuilder
        .SetSizeBytes(renderUtils::alignUniformBufferSizeBytes<CameraCullData>(BUFFERED_FRAMES))
        .Build();
    CameraDataUBOExtended.Buffer = uboBuilder
        .SetSizeBytes(renderUtils::alignUniformBufferSizeBytes<CameraCullData>(BUFFERED_FRAMES))
        .Build();

    BatchIndirectDispatches = indirectDispatchBuilder
        .SetSizeBytes(renderUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(
            maxBatchDispatches * BUFFERED_FRAMES))
        .Build();

    BatchCompactIndirectDispatches = indirectDispatchBuilder
        .SetSizeBytes(renderUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(
            maxBatchDispatches * BUFFERED_FRAMES))
        .Build();

    CountBuffers.VisibleMeshlets = ssboReadBackBuilder
        .SetSizeBytes(renderUtils::alignUniformBufferSizeBytes<u32>(BUFFERED_FRAMES))
        .Build();
    VisibleMeshletCountBufferMappedAddress = CountBuffers.VisibleMeshlets.Map();
    
    CompactedCommands = ssboIndirectBuilder
        .SetSizeBytes(renderPassGeometry.GetCommandCount() * sizeof(IndirectCommand))
        .Build();

    Triangles = ssboBuilder
        .SetSizeBytes(CULL_BATCH_OVERLAP * CullBatch::GetTrianglesSizeBytes())
        .Build();
    
    VisibilityBuffers.MeshVisibility = ssboBuilder
        .SetSizeBytes(renderPassGeometry.GetRenderObjectCount() * sizeof(u16))
        .Build();

    VisibilityBuffers.MeshletVisibility = ssboBuilder
        .SetSizeBytes(renderPassGeometry.GetMeshletCount() * sizeof(u16))
        .Build();

    VisibilityBuffers.TriangleVisibility = ssboBuilder
        .SetSizeBytes(renderPassGeometry.GetMeshletCount() * sizeof(u64))
        .Build();
}

void RenderPassGeometryCull::CullBuffers::Shutdown()
{
    CountBuffers.VisibleMeshlets.Unmap();
}

RenderPassGeometryCull RenderPassGeometryCull::ForGeometry(const RenderPassGeometry& renderPassGeometry,
    DescriptorAllocator& persistentAllocator, DescriptorAllocator& resolutionDependentAllocator)
{
    RenderPassGeometryCull renderPassGeometryCull = {};
    renderPassGeometryCull.m_RenderPassGeometry = &renderPassGeometry;
    renderPassGeometryCull.m_CullBuffers = std::make_shared<CullBuffers>();
    renderPassGeometryCull.m_CullBuffers->Init(renderPassGeometry);
    renderPassGeometryCull.InitPipelines(persistentAllocator, resolutionDependentAllocator);
    renderPassGeometryCull.InitSynchronization();
    
    renderPassGeometryCull.m_BatchedCull = std::make_shared<BatchedCull>();
    renderPassGeometryCull.m_BatchedCull->Init(renderPassGeometry, *renderPassGeometryCull.m_CullBuffers,
        persistentAllocator, resolutionDependentAllocator);

    return renderPassGeometryCull;
}

void RenderPassGeometryCull::Shutdown(const RenderPassGeometryCull& renderPassGeometryCull)
{
    if (!renderPassGeometryCull.m_RenderPassGeometry)
        return;
    renderPassGeometryCull.m_CullBuffers->Shutdown();
    renderPassGeometryCull.m_BatchedCull->Shutdown(renderPassGeometryCull.m_DepthPyramid != nullptr);
}

void RenderPassGeometryCull::SetDepthPyramid(DepthPyramid& depthPyramid, const glm::uvec2& renderResolution)
{
    m_BatchedCull->SetDepthPyramid(depthPyramid, *m_RenderPassGeometry, *m_CullBuffers,
        renderResolution, m_DepthPyramid != nullptr);
    
    m_DepthPyramid = &depthPyramid;
    
    m_MeshCull.Descriptors = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCull.Pipeline.GetTemplate())
        .AddBinding("u_scene_data", m_CullBuffers->CameraDataUBO.Buffer, sizeof(CameraCullData), 0)
        .AddBinding("u_depth_pyramid", depthPyramid.GetTexture().CreateBindingInfo(
            depthPyramid.GetSampler(), ImageLayout::General))
        .AddBinding("u_object_buffer", m_RenderPassGeometry->GetRenderObjectsBuffer())
        .AddBinding("u_object_visibility_buffer", m_CullBuffers->VisibilityBuffers.MeshVisibility)
        .Build();

    m_MeshCullReocclusion.Descriptors = m_MeshCull.Descriptors;

    m_MeshletCull.Descriptors = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCull.Pipeline.GetTemplate())
        .AddBinding("u_scene_data", m_CullBuffers->CameraDataUBO.Buffer, sizeof(CameraCullData), 0)
        .AddBinding("u_depth_pyramid", depthPyramid.GetTexture().CreateBindingInfo(
            depthPyramid.GetSampler(), ImageLayout::General))
        .AddBinding("u_object_buffer", m_RenderPassGeometry->GetRenderObjectsBuffer())
        .AddBinding("u_object_visibility_buffer", m_CullBuffers->VisibilityBuffers.MeshVisibility)
        .AddBinding("u_meshlet_buffer", m_RenderPassGeometry->GetMeshletsBuffer())
        .AddBinding("u_meshlet_visibility_buffer", m_CullBuffers->VisibilityBuffers.MeshletVisibility)
        .AddBinding("u_command_buffer", m_RenderPassGeometry->GetCommandsBuffer())
        .AddBinding("u_compacted_command_buffer", m_CullBuffers->CompactedCommands)
        .AddBinding("u_count_buffer", m_CullBuffers->CountBuffers.VisibleMeshlets, sizeof(u32), 0)
        .Build();

    m_MeshletCullReocclusion.Descriptors = m_MeshletCull.Descriptors;
}

void RenderPassGeometryCull::Prepare(const Camera& camera,
    ResourceUploader& resourceUploader, const FrameContext& frameContext)
{
    auto& sceneCullData = m_CullBuffers->CameraDataUBO.CameraData;
    auto& sceneCullDataExtended = m_CullBuffers->CameraDataUBOExtended.CameraData;

    sceneCullData.FrustumPlanes = camera.GetFrustumPlanes();
    sceneCullData.ProjectionData = camera.GetProjectionData();
    sceneCullData.ViewMatrix = camera.GetView();

    sceneCullDataExtended.FrustumPlanes = sceneCullData.FrustumPlanes;
    sceneCullDataExtended.ProjectionData = sceneCullData.ProjectionData;
    // actually is view-projection
    sceneCullDataExtended.ViewMatrix = camera.GetViewProjection();

    if (m_DepthPyramid != nullptr)
    {
        sceneCullData.PyramidWidth = (f32)m_DepthPyramid->GetTexture().GetDescription().Width;
        sceneCullData.PyramidHeight = (f32)m_DepthPyramid->GetTexture().GetDescription().Height;

        sceneCullDataExtended.PyramidWidth = (f32)m_DepthPyramid->GetTexture().GetDescription().Width;
        sceneCullDataExtended.PyramidHeight = (f32)m_DepthPyramid->GetTexture().GetDescription().Height;
    }
    resourceUploader.UpdateBuffer(m_CullBuffers->CameraDataUBO.Buffer, &sceneCullData,
        sizeof(sceneCullData), renderUtils::alignUniformBufferSizeBytes(sizeof(sceneCullData)) * frameContext.FrameNumber);
    resourceUploader.UpdateBuffer(m_CullBuffers->CameraDataUBOExtended.Buffer, &sceneCullDataExtended,
        sizeof(sceneCullDataExtended), renderUtils::alignUniformBufferSizeBytes(sizeof(sceneCullDataExtended)) *
        frameContext.FrameNumber);
}

void RenderPassGeometryCull::CullRender(const RenderPassGeometryCullRenderingContext& context)
{
    Barrier barrier = {};
    u32 batchCount = 0;
    
    auto preTriangleCull = [&](CommandBuffer& cmd, bool reocclusion, Fence fence)
    {
        ZoneScopedN("Culling");

        CullContextExtended cullContext = {
            .Reocclusion = reocclusion,
            .Cmd = &cmd,
            .FrameNumber = context.FrameNumber,
            .DeletionQueue = context.DeletionQueue};
        
        CullMeshes(cullContext);
        CullMeshlets(cullContext);

        DependencyInfo dependencyInfo = DependencyInfo::Builder()
            .MemoryDependency({
                .SourceStage = PipelineStage::ComputeShader,
                .DestinationStage = PipelineStage::Host,
                .SourceAccess = PipelineAccess::WriteShader,
                .DestinationAccess = PipelineAccess::ReadHost})
            .Build(*cullContext.DeletionQueue);
        barrier.Wait(cmd, dependencyInfo);

        cmd.End();
        cmd.Submit(Driver::GetDevice().GetQueues().Graphics, fence);
    };
    auto preTriangleWaitCPU = [&](CommandBuffer& cmd, Fence fence)
    {
        ZoneScopedN("Fence wait");
        fence.Wait();
        fence.Reset();
        batchCount = m_BatchedCull->ReadBackBatchCount(*m_CullBuffers, context.FrameNumber);
        cmd.Begin();
    };
    auto cull = [&](CommandBuffer& cmd, bool reocclusion)
    {
        ZoneScopedN("Culling");
        CullContextExtended cullContext = {
            .Reocclusion = reocclusion,
            .Cmd = &cmd,
            .FrameNumber = context.FrameNumber,
            .DeletionQueue = context.DeletionQueue};
    
        CullTriangles(cullContext);
    };
    auto postCullBatchBarriers = [&](CommandBuffer& cmd)
    {
        DependencyInfo dependencyInfo = DependencyInfo::Builder()
            .MemoryDependency({
                .SourceStage = PipelineStage::ComputeShader,
                .DestinationStage = PipelineStage::Indirect,
                .SourceAccess = PipelineAccess::WriteShader,
                .DestinationAccess = PipelineAccess::ReadIndirect})
            .Build(*context.DeletionQueue);
        barrier.Wait(cmd, dependencyInfo);
    };
    auto render = [&](CommandBuffer& cmd, u32 iteration, bool computeDepthPyramid, bool shouldClear)
    {
        ZoneScopedN("Rendering");

        RenderCommand::SetViewport(cmd, context.Resolution);
        RenderCommand::SetScissors(cmd, {0, 0}, context.Resolution);
        RenderCommand::BeginRendering(cmd, iteration == 0 && shouldClear ?
            *context.ClearRenderingInfo :
            *context.CopyRenderingInfo);

        RenderCommand::BindIndexU32Buffer(cmd, m_BatchedCull->GetCullDrawBatch().Indices, 0);
        
        context.RenderingPipeline->Pipeline.BindGraphics(cmd);
        PipelineLayout layout = context.RenderingPipeline->Pipeline.GetLayout();
        DescriptorsOffsets offsets = CreateDescriptorOffsets(context);
        context.RenderingPipeline->Descriptors.BindGraphics(cmd, DescriptorKind::Global,
            layout, offsets[(u32)DescriptorKind::Global]);
        context.RenderingPipeline->Descriptors.BindGraphics(cmd, DescriptorKind::Pass,
            layout, offsets[(u32)DescriptorKind::Pass]);
        context.RenderingPipeline->Descriptors.BindGraphics(cmd, DescriptorKind::Material,
            layout, offsets[(u32)DescriptorKind::Material]);
        RenderCommand::DrawIndexedIndirect(cmd,
            m_BatchedCull->GetCullDrawBatch().Draw,
            0, 1, sizeof(IndirectCommand));

        RenderCommand::EndRendering(cmd);

        if (computeDepthPyramid)
        {
            ZoneScopedN("Compute depth pyramid");
            m_DepthPyramid->Compute(*context.DepthBuffer, cmd, *context.DeletionQueue);
        }
    };
    auto triangleCullRenderLoop = [&](CommandBuffer& cmd, bool reocclusion, bool computeDepthPyramid, bool shouldClear)
    {
        ResetSubBatches();
        u32 iterations = std::max(batchCount, 1u);
        for (u32 i = 0; i < iterations; i++)
        {
            m_SplitBarriers[m_BatchedCull->GetBatchIndex()].Wait(cmd, m_SplitBarrierDependency);
            m_SplitBarriers[m_BatchedCull->GetBatchIndex()].Reset(cmd, m_SplitBarrierDependency);
            cull(cmd, reocclusion);
            postCullBatchBarriers(cmd);
            render(cmd, i, computeDepthPyramid && i == iterations - 1, shouldClear);
            m_SplitBarriers[m_BatchedCull->GetBatchIndex()].Signal(cmd, m_SplitBarrierDependency);
            m_BatchedCull->NextSubBatch();
        }
    };
    auto postRenderBarriers = [&](CommandBuffer& cmd)
    {
        DependencyInfo dependencyInfo = DependencyInfo::Builder()
            .MemoryDependency({
                .SourceStage = PipelineStage::ColorOutput,
                .DestinationStage = PipelineStage::PixelShader,
                .SourceAccess = PipelineAccess::WriteColorAttachment,
                .DestinationAccess = PipelineAccess::ReadShader})
            .Build(*context.DeletionQueue);
        barrier.Wait(cmd, dependencyInfo);
    };

    Fence fence = Fence::Builder().BuildManualLifetime();
    
    CommandBuffer& cmd = *context.Cmd;

    if (m_ShouldCreateSplitBarriers)
    {
        for (auto& splitBarrier : m_SplitBarriers)
        {
            splitBarrier = SplitBarrier::Builder().Build();
            splitBarrier.Signal(cmd, m_SplitBarrierDependency);
        }
        m_ShouldCreateSplitBarriers = false;
    }
    
    preTriangleCull(cmd, false, fence);
    preTriangleWaitCPU(cmd, fence);

    BatchIndirectDispatchesBuffersPrepare({
        .Cmd = &cmd,
        .FrameNumber = context.FrameNumber,
        .DeletionQueue = context.DeletionQueue,});
    triangleCullRenderLoop(cmd, false, true, true);

    // triangle-only reocclusion
    triangleCullRenderLoop(cmd, true, true, false);

    // meshlet reocclusion
    preTriangleCull(cmd, true, fence);
    preTriangleWaitCPU(cmd, fence);
    BatchIndirectDispatchesBuffersPrepare({
        .Cmd = &cmd,
        .FrameNumber = context.FrameNumber,
        .DeletionQueue = context.DeletionQueue});
    triangleCullRenderLoop(cmd, true, false, false);
    postRenderBarriers(cmd);

    Fence::Destroy(fence);
}

const Buffer& RenderPassGeometryCull::GetTriangleBuffer() const
{
    return m_CullBuffers->Triangles;
}

u32 RenderPassGeometryCull::GetTriangleBufferSizeBytes() const
{
    return (u32)CullBatch::GetTrianglesSizeBytes();
}

u32 RenderPassGeometryCull::GetTriangleBufferOffset() const
{
    return (u32)m_BatchedCull->GetTrianglesOffset();
}

void RenderPassGeometryCull::InitPipelines(DescriptorAllocator& persistentAllocator,
    DescriptorAllocator& resolutionDependentAllocator)
{
    ShaderPipelineTemplate* meshCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/mesh-cull-comp.shader"}, "mesh-cull",
        resolutionDependentAllocator);

    ShaderPipelineTemplate* meshletCullTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/meshlet-cull-comp.shader"}, "meshlet-cull",
        resolutionDependentAllocator);

    ShaderPipelineTemplate* meshletClearTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/clear-buffers-comp.shader"}, "clear-buffers-cull",
        persistentAllocator);

    m_MeshCull.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshCullTemplate)
        .Build();
    m_MeshCullReocclusion.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshCullTemplate)
        .AddSpecialization("REOCCLUSION", true)
        .Build();

    m_MeshletCull.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshletCullTemplate)
        .Build();
    m_MeshletCullReocclusion.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(meshletCullTemplate)
        .AddSpecialization("REOCCLUSION", true)
        .Build();

    m_MeshletCullClear.Pipeline = ShaderPipeline::Builder()
       .SetTemplate(meshletClearTemplate)
       .Build();
    m_MeshletCullClear.Descriptors = ShaderDescriptorSet::Builder()
        .SetTemplate(meshletClearTemplate)
        .AddBinding("u_count_buffer", m_CullBuffers->CountBuffers.VisibleMeshlets, sizeof(u32), 0)
        .Build();
}

void RenderPassGeometryCull::InitSynchronization()
{
    m_ComputeWRDependency = DependencyInfo::Builder()
        .MemoryDependency({
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage = PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadSampled})
        .Build();

    m_SplitBarrierDependency = DependencyInfo::Builder()
        .MemoryDependency({
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage = PipelineStage::PixelShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadStorage})
        .Build();
}

void RenderPassGeometryCull::CullMeshes(const CullContextExtended& cullContext) const
{
    const CommandBuffer& cmd = *cullContext.Cmd;

    u32 sceneCullOffset = (u32)renderUtils::alignUniformBufferSizeBytes(sizeof(CameraCullData)) *
        cullContext.FrameNumber;
    u32 count = m_RenderPassGeometry->GetRenderObjectCount();

    const RenderGraph::PipelineData& pipelineData = cullContext.Reocclusion ? m_MeshCullReocclusion : m_MeshCull;

    pipelineData.Pipeline.BindCompute(cmd);
    pipelineData.Descriptors.BindCompute(cmd, DescriptorKind::Global,
        pipelineData.Pipeline.GetLayout(), {sceneCullOffset});
    RenderCommand::PushConstants(cmd, pipelineData.Pipeline.GetLayout(), &count);
    RenderCommand::Dispatch(cmd, {count / 64 + 1, 1, 1});

    m_Barrier.Wait(cmd, m_ComputeWRDependency);
}

void RenderPassGeometryCull::CullMeshlets(const CullContextExtended& cullContext) const
{
    const CommandBuffer& cmd = *cullContext.Cmd;
    
    u32 countBufferOffset = (u32)renderUtils::alignUniformBufferSizeBytes(sizeof(u32)) * cullContext.FrameNumber;
    m_MeshletCullClear.Pipeline.BindCompute(cmd);
    m_MeshletCullClear.Descriptors.BindCompute(cmd, DescriptorKind::Global,
                                               m_MeshletCullClear.Pipeline.GetLayout(), {countBufferOffset});
    RenderCommand::Dispatch(cmd, {1, 1, 1});
    m_Barrier.Wait(cmd, m_ComputeWRDependency);

    u32 sceneCullOffset = (u32)renderUtils::alignUniformBufferSizeBytes(sizeof(CameraCullData)) *
        cullContext.FrameNumber;
    u32 count = m_RenderPassGeometry->GetMeshletCount();

    const RenderGraph::PipelineData& pipelineData = cullContext.Reocclusion ? m_MeshletCullReocclusion : m_MeshletCull;

    pipelineData.Pipeline.BindCompute(cmd);
    pipelineData.Descriptors.BindCompute(cmd, DescriptorKind::Global,
                                         pipelineData.Pipeline.GetLayout(), {sceneCullOffset, countBufferOffset});
    RenderCommand::PushConstants(cmd, pipelineData.Pipeline.GetLayout(), &count);
    RenderCommand::Dispatch(cmd, {count / 64 + 1, 1, 1});

    m_Barrier.Wait(cmd, m_ComputeWRDependency);
}

void RenderPassGeometryCull::BatchIndirectDispatchesBuffersPrepare(const CullContextExtended& cullContext) const
{
    m_BatchedCull->BatchIndirectDispatchesBuffersPrepare(cullContext);
}

void RenderPassGeometryCull::CullTriangles(const CullContextExtended& cullContext) const
{
    m_BatchedCull->CullTriangles(cullContext);
}

void RenderPassGeometryCull::NextSubBatch() const
{
    m_BatchedCull->NextSubBatch();
}

void RenderPassGeometryCull::ResetSubBatches() const
{
    m_BatchedCull->ResetSubBatches();
}

DescriptorsOffsets RenderPassGeometryCull::CreateDescriptorOffsets(
    const RenderPassGeometryCullRenderingContext& context) const
{
    DescriptorsOffsets offsets = context.DescriptorsOffsets;
    for (auto& set : offsets)
        for (auto& offset : set)
            if (offset == TRIANGLE_OFFSET)
                offset = GetTriangleBufferOffset();

    return offsets;
}
