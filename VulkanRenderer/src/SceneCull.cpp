#include "SceneCull.h"

#include "Renderer.h"
#include "ResourceUploader.h"
#include "Scene.h"
#include "Settings.h"
#include "utils/MathUtils.h"
#include "utils/SceneUtils.h"
#include "Vulkan/DepthPyramid.h"
#include "Vulkan/VulkanUtils.h"

void SceneCullBuffers::Init()
{
    Buffer::Builder uboBuilder = Buffer::Builder()
        .SetKind({BufferKind::Uniform})
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    Buffer::Builder ssboIndirectBuilder = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    
    m_CullDataUBO.Buffer = uboBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<SceneCullData>(BUFFERED_FRAMES))
        .Build();
    m_CullDataUBOExtended.Buffer = uboBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<SceneCullDataExtended>(BUFFERED_FRAMES))
        .Build();

    u32 maxBatchDispatches = MAX_DRAW_INDIRECT_CALLS / (CullDrawBatch::MAX_COMMANDS * SUB_BATCH_COUNT);
    
    m_BatchIndirectDispatches = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(maxBatchDispatches * BUFFERED_FRAMES))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_BatchClearIndirectDispatches = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(maxBatchDispatches * BUFFERED_FRAMES))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_CompactedCommands = ssboIndirectBuilder
        .SetSizeBytes(sizeof(IndirectCommand) * MAX_COMMAND_COUNT)
        .Build();

    m_CompactedBatchCommands = ssboIndirectBuilder
        .SetSizeBytes(sizeof(IndirectCommand) *  CullDrawBatch::MAX_COMMANDS * SUB_BATCH_COUNT * CULL_DRAW_BATCH_OVERLAP)
        .Build();

    m_CountBuffers.VisibleMeshlets = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<u32>(BUFFERED_FRAMES))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)
        .Build();
    m_VisibleMeshletCountBufferMappedAddress = m_CountBuffers.VisibleMeshlets.Map();

    m_VisibilityBuffers.MeshVisibility = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(MAX_OBJECTS * sizeof(u16))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_VisibilityBuffers.MeshletVisibility = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(MAX_DRAW_INDIRECT_CALLS * sizeof(u16))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();
    
    m_VisibilityBuffers.TriangleVisibility = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(sizeof(u64) * MAX_DRAW_INDIRECT_CALLS)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();
}

void SceneCullBuffers::Shutdown()
{
    m_CountBuffers.VisibleMeshlets.Unmap();
}

void SceneCullBuffers::Update(const Camera& camera, const DepthPyramid* depthPyramid,  ResourceUploader& resourceUploader, const FrameContext& frameContext)
{
    auto& sceneCullData = m_CullDataUBO.SceneData;
    auto& sceneCullDataExtended = m_CullDataUBOExtended.SceneData;

    sceneCullData.FrustumPlanes = camera.GetFrustumPlanes();
    sceneCullData.ProjectionData = camera.GetProjectionData();
    sceneCullData.ViewMatrix = camera.GetView();

    sceneCullDataExtended.FrustumPlanes = sceneCullData.FrustumPlanes;
    sceneCullDataExtended.ProjectionData = sceneCullData.ProjectionData;
    sceneCullDataExtended.ViewProjectionMatrix = camera.GetViewProjection();

    if (depthPyramid)
    {
        sceneCullData.PyramidWidth = (f32)depthPyramid->GetTexture().GetImageData().Width;
        sceneCullData.PyramidHeight = (f32)depthPyramid->GetTexture().GetImageData().Height;

        sceneCullDataExtended.PyramidWidth = (f32)depthPyramid->GetTexture().GetImageData().Width;
        sceneCullDataExtended.PyramidHeight = (f32)depthPyramid->GetTexture().GetImageData().Height;
    }
    resourceUploader.UpdateBuffer(m_CullDataUBO.Buffer, &sceneCullData,
        sizeof(sceneCullData), vkUtils::alignUniformBufferSizeBytes(sizeof(sceneCullData)) * frameContext.FrameNumber);
    resourceUploader.UpdateBuffer(m_CullDataUBOExtended.Buffer, &sceneCullDataExtended,
        sizeof(sceneCullDataExtended), vkUtils::alignUniformBufferSizeBytes(sizeof(sceneCullDataExtended)) * frameContext.FrameNumber);
}

u32 SceneCullBuffers::GetVisibleMeshletsCountValue(u32 frameNumber) const
{
    void* address = m_VisibleMeshletCountBufferMappedAddress;
    address = (u8*)address + vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameNumber;
    return *(u32*)address;
}


CullDrawBatch::CullDrawBatch()
{
    m_Count = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage})
        .SetSizeBytes(sizeof(u32))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .BuildManualLifetime();

    m_Indices = Buffer::Builder()
        .SetKinds({BufferKind::Index, BufferKind::Storage})
        .SetSizeBytes(MAX_INDICES * sizeof(assetLib::ModelInfo::IndexType) * SUB_BATCH_COUNT)
        .BuildManualLifetime();
}

CullDrawBatch::~CullDrawBatch()
{
    Buffer::Destroy(m_Count);
    Buffer::Destroy(m_Indices);
}

SceneBatchedCull::SceneBatchedCull(Scene& scene, SceneCullBuffers& sceneCullBuffers)
{
    m_Scene = &scene;
    m_SceneCullBuffers = &sceneCullBuffers;

    m_ComputeWRBarrierBase = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Compute,
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};

    m_IndirectWRBarrierBase = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Compute,
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT};

    for (auto& batch : m_CullDrawBatches)
        batch = std::make_unique<CullDrawBatch>();

    m_MaxBatchDispatches = MAX_DRAW_INDIRECT_CALLS / CullDrawBatch::GetCommandCount() + 1;
}

void SceneBatchedCull::Init(DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    InitBatchCull(allocator, layoutCache);
}

void SceneBatchedCull::Shutdown()
{
    if (m_CullIsInitialized)
        DestroyDescriptors();

    for (auto& batch : m_CullDrawBatches)
        batch.reset();
}

void SceneBatchedCull::SetDepthPyramid(const DepthPyramid& depthPyramid, const Buffer& triangles, u64 trianglesSizeBytes, u64 trianglesOffset)
{
    if (m_CullIsInitialized)
        DestroyDescriptors();
    else
        m_CullIsInitialized = true;

    for (u32 i = 0; i < CULL_DRAW_BATCH_OVERLAP; i++)
    {
        auto& batchData = m_CullDrawBatchData[i];

        batchData.TriangleCull.DescriptorSet = ShaderDescriptorSet::Builder()
            .SetTemplate(batchData.TriangleCull.Template)
            .AddBinding("u_scene_data", m_SceneCullBuffers->GetCullDataExtended(), sizeof(SceneCullBuffers::SceneCullDataExtended), 0)
            .AddBinding("u_depth_pyramid", {
                .View = depthPyramid.GetTexture().GetImageData().View,
                .Sampler = depthPyramid.GetSampler(),
                .Layout = VK_IMAGE_LAYOUT_GENERAL})
            .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
            .AddBinding("u_meshlet_visibility_buffer", m_SceneCullBuffers->GetMeshletVisibility())
            .AddBinding("u_positions_buffer", m_Scene->GetPositionsBuffer())
            .AddBinding("u_indices_buffer", m_Scene->GetIndicesBuffer())
            .AddBinding("u_culled_indices_buffer", m_CullDrawBatches[i]->GetIndices())
            .AddBinding("u_visible_triangles_buffer", m_SceneCullBuffers->GetTriangleVisibility())
            .AddBinding("u_triangle_buffer", triangles, trianglesSizeBytes, trianglesOffset)
            .AddBinding("u_command_buffer", m_SceneCullBuffers->GetCompactedCommands())
            .AddBinding("u_count_buffer", m_SceneCullBuffers->GetVisibleMeshletCount(), sizeof(u32), 0)
            .BuildManualLifetime();

        batchData.TriangleCullReocclusion.DescriptorSet = batchData.TriangleCull.DescriptorSet;
    }
}

void SceneBatchedCull::BatchIndirectDispatchesBuffersPrepare(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.ComputeCommandBuffers.GetBuffer();
    
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    u32 dispatchIndirectOffset = (u32)vkUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(m_MaxBatchDispatches) * frameContext.FrameNumber;

    std::vector<u32> pushConstants = {
        CullDrawBatch::GetCommandCount(),
        assetLib::ModelInfo::TRIANGLES_PER_MESHLET,
        Driver::GetSubgroupSize(),
        m_MaxBatchDispatches};
    PushConstantDescription pushConstantDescription = m_PrepareIndirectDispatches.Pipeline.GetPushConstantDescription();
    
    m_PrepareIndirectDispatches.Pipeline.BindCompute(cmd);
    m_PrepareIndirectDispatches.DescriptorSet.BindCompute(cmd, DescriptorKind::Global, m_PrepareIndirectDispatches.Pipeline.GetPipelineLayout(), {countOffset, dispatchIndirectOffset});
    RenderCommand::PushConstants(cmd, m_PrepareIndirectDispatches.Pipeline.GetPipelineLayout(), pushConstants.data(), pushConstantDescription);
    RenderCommand::Dispatch(cmd, {m_MaxBatchDispatches / 64 + 1, 1, 1});

    pushConstants = {
        CullDrawBatch::GetCommandCount(),
        1,
        Driver::GetSubgroupSize(),
        m_MaxBatchDispatches};
    pushConstantDescription = m_PrepareCompactIndirectDispatches.Pipeline.GetPushConstantDescription();
    m_PrepareCompactIndirectDispatches.Pipeline.BindCompute(cmd);
    m_PrepareCompactIndirectDispatches.DescriptorSet.BindCompute(cmd, DescriptorKind::Global, m_PrepareCompactIndirectDispatches.Pipeline.GetPipelineLayout(), {countOffset, dispatchIndirectOffset});
    RenderCommand::PushConstants(cmd, m_PrepareCompactIndirectDispatches.Pipeline.GetPipelineLayout(), pushConstants.data(), pushConstantDescription);
    RenderCommand::Dispatch(cmd, {m_MaxBatchDispatches / 64 + 1, 1, 1});

    m_IndirectWRBarrierBase.Buffer = &m_SceneCullBuffers->GetBatchIndirectDispatches();
    RenderCommand::CreateBarrier(cmd, m_IndirectWRBarrierBase);

    m_IndirectWRBarrierBase.Buffer = &m_SceneCullBuffers->GetBatchCompactIndirectDispatches();
    RenderCommand::CreateBarrier(cmd, m_IndirectWRBarrierBase);
}

void SceneBatchedCull::CullTriangles(const FrameContext& frameContext, const CullSettings& cullSettings)
{
    const CommandBuffer& cmd = frameContext.ComputeCommandBuffers.GetBuffer();

    ComputeBatchData& batchData = m_CullDrawBatchData[m_CurrentBatch];
    CullDrawBatch& batch = *m_CullDrawBatches[m_CurrentBatch];
    u32 commandCount = CullDrawBatch::GetCommandCount();

    u64 dispatchIndirectOffset = vkUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(m_MaxBatchDispatches) * frameContext.FrameNumber;
    dispatchIndirectOffset += sizeof(VkDispatchIndirectCommand) * m_CurrentBatchFlat;
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    u32 commandOffset = commandCount * m_CurrentBatchFlat;
    u32 compactedCommandsOffsetBytes = (u32)CullDrawBatch::GetCommandsSizeBytes() * m_CurrentBatch;
    
    std::vector<u32> pushConstants {commandCount, commandOffset, SceneCullBuffers::MAX_COMMAND_COUNT};
    
    ComputePipelineData& batchClearCommands = batchData.ClearCommands;

    PushConstantDescription pushConstantDescription = batchClearCommands.Pipeline.GetPushConstantDescription();
    batchClearCommands.Pipeline.BindCompute(cmd);
    batchClearCommands.DescriptorSet.BindCompute(cmd, DescriptorKind::Global, batchClearCommands.Pipeline.GetPipelineLayout(), {compactedCommandsOffsetBytes, countOffset});
    RenderCommand::PushConstants(cmd, batchClearCommands.Pipeline.GetPipelineLayout(), pushConstants.data(), pushConstantDescription);
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers->GetBatchCompactIndirectDispatches(), dispatchIndirectOffset);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers->GetCompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullDataExtended)) * frameContext.FrameNumber;
    u32 triangleOffset = (u32)batch.GetTrianglesSizeBytes() * m_CurrentBatch;
    
    ComputePipelineData& pipelineData = cullSettings.Reocclusion ? batchData.TriangleCullReocclusion : batchData.TriangleCull;
    
    glm::uvec2 resolution = frameContext.Resolution;
    pushConstantDescription = pipelineData.Pipeline.GetPushConstantDescription();

    pipelineData.Pipeline.BindCompute(cmd);
    pipelineData.DescriptorSet.BindCompute(cmd, DescriptorKind::Global, pipelineData.Pipeline.GetPipelineLayout(), {sceneCullOffset, triangleOffset, countOffset});

    for (u32 i = 0; i < assetLib::ModelInfo::TRIANGLES_PER_MESHLET / Driver::GetSubgroupSize(); i++)
    {
        std::vector<u32> pushConstantsCull(pushConstantDescription.GetSizeBytes() / sizeof(u32));
        pushConstantsCull[0] = resolution.x; 
        pushConstantsCull[1] = resolution.y; 
        pushConstantsCull[2] = i * Driver::GetSubgroupSize(); 
        pushConstantsCull[3] = commandOffset;   
        pushConstantsCull[4] = commandCount; 
        pushConstantsCull[5] = SceneCullBuffers::MAX_COMMAND_COUNT; 
        RenderCommand::PushConstants(cmd, pipelineData.Pipeline.GetPipelineLayout(), pushConstantsCull.data(), pushConstantDescription);
        RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers->GetBatchIndirectDispatches(), dispatchIndirectOffset);
    }

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers->GetCompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &batch.GetIndices();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    ComputePipelineData& batchClear = batchData.ClearCount;
    batchClear.Pipeline.BindCompute(cmd);
    batchClear.DescriptorSet.BindCompute(cmd, DescriptorKind::Global, batchClear.Pipeline.GetPipelineLayout(), {0});
    RenderCommand::Dispatch(cmd, {1, 1, 1});
        
    m_ComputeWRBarrierBase.Buffer = &batch.GetCountBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
    
    ComputePipelineData& batchCompact = batchData.CompactCommands;
    
    pushConstantDescription = batchCompact.Pipeline.GetPushConstantDescription();
    pushConstants = {commandCount, commandOffset, SceneCullBuffers::MAX_COMMAND_COUNT};

    batchCompact.Pipeline.BindCompute(cmd);
    batchCompact.DescriptorSet.BindCompute(cmd, DescriptorKind::Global, batchCompact.Pipeline.GetPipelineLayout(), {compactedCommandsOffsetBytes, countOffset});
    RenderCommand::PushConstants(cmd, batchCompact.Pipeline.GetPipelineLayout(), pushConstants.data(), pushConstantDescription);
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers->GetBatchCompactIndirectDispatches(), dispatchIndirectOffset);
}

void SceneBatchedCull::NextSubBatch()
{
    m_CurrentBatchFlat++;
    m_CurrentBatch = (m_CurrentBatchFlat) % CULL_DRAW_BATCH_OVERLAP;
}

void SceneBatchedCull::ResetSubBatches()
{
    m_CurrentBatchFlat = 0;
    m_CurrentBatch = 0;
}

const Buffer& SceneBatchedCull::GetDrawCount() const
{
    return m_CullDrawBatches[m_CurrentBatch]->GetCountBuffer();
}

u32 SceneBatchedCull::ReadBackBatchCount(const FrameContext& frameContext) const
{
    u32 visibleMeshletsValue = m_SceneCullBuffers->GetVisibleMeshletsCountValue(frameContext.FrameNumber);
    u32 commandCount = CullDrawBatch::GetCommandCount();

    return visibleMeshletsValue / commandCount + (u32)(visibleMeshletsValue % commandCount != 0); 
}

void SceneBatchedCull::DestroyDescriptors()
{
    for (u32 i = 0; i < CULL_DRAW_BATCH_OVERLAP; i++)
    {
        auto& batchData = m_CullDrawBatchData[i];
        ShaderDescriptorSet::Destroy(batchData.TriangleCull.DescriptorSet);
    }
}

void SceneBatchedCull::InitBatchCull(DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    auto& firstBatchData = m_CullDrawBatchData.front();

    if (Driver::GetSubgroupSize() == 64)
        firstBatchData.TriangleCull.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
            {"../assets/shaders/processed/culling/triangle-cull-amd-comp.shader"}, "triangle-cull-amd",
            allocator, layoutCache);
    else
        firstBatchData.TriangleCull.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
            {"../assets/shaders/processed/culling/triangle-cull-nvidia-comp.shader"}, "triangle-cull-nvidia",
            allocator, layoutCache);
        
    firstBatchData.TriangleCull.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(firstBatchData.TriangleCull.Template)
        .Build();

    firstBatchData.TriangleCullReocclusion.Template = firstBatchData.TriangleCull.Template;

    firstBatchData.TriangleCullReocclusion.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(firstBatchData.TriangleCull.Template)
        .AddSpecialization("REOCCLUSION", true)
        .Build();

    firstBatchData.CompactCommands.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compact-commands-comp.shader"}, "compact-commands",
        allocator, layoutCache);

    firstBatchData.CompactCommands.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(firstBatchData.CompactCommands.Template)
        .Build();

    firstBatchData.ClearCommands.Template = firstBatchData.CompactCommands.Template;

    firstBatchData.ClearCommands.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(firstBatchData.ClearCommands.Template)
        .AddSpecialization("CLEAR", true)
        .Build();
    
    firstBatchData.ClearCount.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/clear-buffers-comp.shader"}, "clear-buffers-cull",
        allocator, layoutCache);

    firstBatchData.ClearCount.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(firstBatchData.ClearCount.Template)
        .Build();
    
    for (u32 i = 1; i < CULL_DRAW_BATCH_OVERLAP; i++)
    {
        m_CullDrawBatchData[i].TriangleCull.Template = firstBatchData.TriangleCull.Template;
        m_CullDrawBatchData[i].TriangleCull.Pipeline = firstBatchData.TriangleCull.Pipeline;
        
        m_CullDrawBatchData[i].TriangleCullReocclusion.Template = firstBatchData.TriangleCullReocclusion.Template;
        m_CullDrawBatchData[i].TriangleCullReocclusion.Pipeline = firstBatchData.TriangleCullReocclusion.Pipeline;
        
        m_CullDrawBatchData[i].CompactCommands.Template = firstBatchData.CompactCommands.Template;
        m_CullDrawBatchData[i].CompactCommands.Pipeline = firstBatchData.CompactCommands.Pipeline;
        
        m_CullDrawBatchData[i].ClearCommands.Template = firstBatchData.ClearCommands.Template;
        m_CullDrawBatchData[i].ClearCommands.Pipeline = firstBatchData.ClearCommands.Pipeline;

        m_CullDrawBatchData[i].ClearCount.Template = firstBatchData.ClearCount.Template;
        m_CullDrawBatchData[i].ClearCount.Pipeline = firstBatchData.ClearCount.Pipeline;
    }
    
    for (u32 i = 0; i < CULL_DRAW_BATCH_OVERLAP; i++)
    {
        auto& batchData = m_CullDrawBatchData[i];

        batchData.CompactCommands.DescriptorSet = ShaderDescriptorSet::Builder()
            .SetTemplate(batchData.CompactCommands.Template)
            .AddBinding("u_command_buffer", m_SceneCullBuffers->GetCompactedCommands())
            .AddBinding("u_compacted_command_buffer", m_SceneCullBuffers->GetCompactedBatchCommands(),
                CullDrawBatch::GetCommandsSizeBytes(), 0)
            .AddBinding("u_command_count_buffer", m_SceneCullBuffers->GetVisibleMeshletCount(), sizeof(u32), 0)
            .AddBinding("u_compacted_command_count_buffer", m_CullDrawBatches[i]->GetCountBuffer())
            .Build();

        batchData.ClearCommands.DescriptorSet = ShaderDescriptorSet::Builder()
            .SetTemplate(batchData.CompactCommands.Template)
            .AddBinding("u_command_buffer", m_SceneCullBuffers->GetCompactedCommands())
            .AddBinding("u_compacted_command_buffer", m_SceneCullBuffers->GetCompactedBatchCommands(),
                CullDrawBatch::GetCommandsSizeBytes(), 0)
            .AddBinding("u_command_count_buffer", m_SceneCullBuffers->GetVisibleMeshletCount(), sizeof(u32), 0)
            .AddBinding("u_compacted_command_count_buffer", m_CullDrawBatches[i]->GetCountBuffer())
            .Build();

        batchData.ClearCount.DescriptorSet = ShaderDescriptorSet::Builder()
            .SetTemplate(batchData.ClearCount.Template)
            .AddBinding("u_count_buffer", m_CullDrawBatches[i]->GetCountBuffer())
            .Build();
    }

    m_PrepareIndirectDispatches.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/prepare-indirect-dispatches-comp.shader"}, "prepare-batch-indirect-dispatch",
        allocator, layoutCache);

    m_PrepareIndirectDispatches.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_PrepareIndirectDispatches.Template)
        .Build();

    m_PrepareIndirectDispatches.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_PrepareIndirectDispatches.Template)
        .AddBinding("u_command_count_buffer", m_SceneCullBuffers->GetVisibleMeshletCount(), sizeof(u32), 0)
        .AddBinding("u_indirect_dispatch_buffer", m_SceneCullBuffers->GetBatchIndirectDispatches(), m_MaxBatchDispatches * sizeof(VkDispatchIndirectCommand), 0)
        .Build();

    m_PrepareCompactIndirectDispatches.Template = m_PrepareIndirectDispatches.Template;

    m_PrepareCompactIndirectDispatches.Pipeline = m_PrepareIndirectDispatches.Pipeline;

    m_PrepareCompactIndirectDispatches.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_PrepareCompactIndirectDispatches.Template)
        .AddBinding("u_command_count_buffer", m_SceneCullBuffers->GetVisibleMeshletCount(), sizeof(u32), 0)
        .AddBinding("u_indirect_dispatch_buffer", m_SceneCullBuffers->GetBatchCompactIndirectDispatches(), m_MaxBatchDispatches * sizeof(VkDispatchIndirectCommand), 0)
        .Build();
}

void SceneCull::Init(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_Scene = &scene;
    m_SceneCullBuffers.Init();

    m_SceneBatchedCull = std::make_unique<SceneBatchedCull>(scene, m_SceneCullBuffers);

    m_TrianglesOffsetBase = vkUtils::alignUniformBufferSizeBytes(GetBatchCull().GetCullDrawBatch().GetTrianglesSizeBytes());
    m_Triangles = Buffer::Builder()
        .SetKinds({BufferKind::Storage})
        .SetSizeBytes(m_TrianglesOffsetBase * CULL_DRAW_BATCH_OVERLAP)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_SceneBatchedCull->Init(allocator, layoutCache);

    InitBarriers();
    
    InitMeshCull(allocator, layoutCache);
    InitMeshletCull(allocator, layoutCache);
}

void SceneCull::Shutdown()
{
    m_SceneCullBuffers.Shutdown();
    if (m_CullIsInitialized)
        DestroyDescriptors();

    m_SceneBatchedCull->Shutdown();
}

void SceneCull::DestroyDescriptors()
{
    ShaderDescriptorSet::Destroy(m_MeshCull.DescriptorSet);
    ShaderDescriptorSet::Destroy(m_MeshletCull.DescriptorSet);
}

void SceneCull::SetDepthPyramid(const DepthPyramid& depthPyramid)
{
    m_DepthPyramid = &depthPyramid;

    m_SceneBatchedCull->SetDepthPyramid(depthPyramid, m_Triangles, m_TrianglesOffsetBase, 0);

    if (m_CullIsInitialized)
        DestroyDescriptors();
    else
        m_CullIsInitialized = true;
    
    m_MeshCull.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCull.Template)
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullData(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_object_visibility_buffer", m_SceneCullBuffers.GetMeshVisibility())
        .BuildManualLifetime();

    m_MeshCullReocclusion.DescriptorSet = m_MeshCull.DescriptorSet;

    m_MeshletCull.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCull.Template)
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullData(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_object_visibility_buffer", m_SceneCullBuffers.GetMeshVisibility())
        .AddBinding("u_meshlet_buffer", m_Scene->GetMeshletsBuffer())
        .AddBinding("u_meshlet_visibility_buffer", m_SceneCullBuffers.GetMeshletVisibility())
        .AddBinding("u_command_buffer", m_Scene->GetMeshletsIndirectBuffer())
        .AddBinding("u_compacted_command_buffer", m_SceneCullBuffers.GetCompactedCommands())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleMeshletCount(), sizeof(u32), 0)
        .BuildManualLifetime();

    m_MeshletCullReocclusion.DescriptorSet = m_MeshletCull.DescriptorSet;
}

void SceneCull::UpdateBuffers(const Camera& camera, ResourceUploader& resourceUploader, const FrameContext& frameContext)
{
    m_SceneCullBuffers.Update(camera, m_DepthPyramid, resourceUploader, frameContext);
}

void SceneCull::CullMeshes(const FrameContext& frameContext, bool reocclusion)
{
    const CommandBuffer& cmd = frameContext.ComputeCommandBuffers.GetBuffer();

    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;

    u32 count = (u32)m_Scene->GetRenderObjects().size();

    ComputePipelineData& pipelineData = reocclusion ? m_MeshCullReocclusion : m_MeshCull;

    PushConstantDescription pushConstantDescription = pipelineData.Pipeline.GetPushConstantDescription();
    pipelineData.Pipeline.BindCompute(cmd);
    pipelineData.DescriptorSet.BindCompute(cmd, DescriptorKind::Global, pipelineData.Pipeline.GetPipelineLayout(), {sceneCullOffset});
    RenderCommand::PushConstants(cmd, pipelineData.Pipeline.GetPipelineLayout(), &count, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {count / 64 + 1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetMeshVisibility();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CullMeshlets(const FrameContext& frameContext, bool reocclusion)
{
    const CommandBuffer& cmd = frameContext.ComputeCommandBuffers.GetBuffer();
    
    u32 countBufferOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    m_MeshletCullClear.Pipeline.BindCompute(cmd);
    m_MeshletCullClear.DescriptorSet.BindCompute(cmd, DescriptorKind::Global, m_MeshletCullClear.Pipeline.GetPipelineLayout(), {countBufferOffset});
    RenderCommand::Dispatch(cmd, {1, 1, 1});
    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetVisibleMeshletCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;
    u32 count = m_Scene->GetMeshletCount();

    ComputePipelineData& pipelineData = reocclusion ? m_MeshletCullReocclusion : m_MeshletCull;

    PushConstantDescription pushConstantDescription = pipelineData.Pipeline.GetPushConstantDescription();
    pipelineData.Pipeline.BindCompute(cmd);
    pipelineData.DescriptorSet.BindCompute(cmd, DescriptorKind::Global, pipelineData.Pipeline.GetPipelineLayout(), {sceneCullOffset, countBufferOffset});
    RenderCommand::PushConstants(cmd, pipelineData.Pipeline.GetPipelineLayout(), &count, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {count / 64 + 1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetMeshVisibility();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetCompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetVisibleMeshletCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

const SceneCullBuffers& SceneCull::GetSceneCullBuffers() const
{
    return m_SceneCullBuffers;
}

void SceneCull::BatchIndirectDispatchesBuffersPrepare(const FrameContext& frameContext)
{
    GetBatchCull().BatchIndirectDispatchesBuffersPrepare(frameContext);
}

const CullDrawBatch& SceneCull::GetCullDrawBatch() const
{
    return GetBatchCull().GetCullDrawBatch();
}

void SceneCull::NextSubBatch()
{
    GetBatchCull().NextSubBatch();
}

void SceneCull::ResetSubBatches()
{
    GetBatchCull().ResetSubBatches();
}

const Buffer& SceneCull::GetDrawCommands() const
{
    return m_SceneCullBuffers.GetCompactedBatchCommands();
}

const Buffer& SceneCull::GetTriangles() const
{
    return m_Triangles;
}

u64 SceneCull::GetDrawCommandsOffset() const
{
    return GetBatchCull().GetDrawCommandsOffset();
}

u64 SceneCull::GetDrawTrianglesOffset() const
{
    return GetBatchCull().GetTrianglesOffset();
}

const Buffer& SceneCull::GetDrawCount() const
{
    return GetBatchCull().GetDrawCount();
}

u32 SceneCull::GetMaxBatchCount() const
{
    return GetBatchCull().GetMaxBatchCount();
}

u32 SceneCull::ReadBackBatchCount(const FrameContext& frameContext) const
{
    return GetBatchCull().ReadBackBatchCount(frameContext);
}

SceneBatchedCull& SceneCull::GetBatchCull()
{
    return *m_SceneBatchedCull;
}

const SceneBatchedCull& SceneCull::GetBatchCull() const
{
    return *m_SceneBatchedCull;
}

void SceneCull::CullCompactTrianglesBatch(const FrameContext& frameContext, const CullSettings& cullSettings)
{
    GetBatchCull().CullTriangles(frameContext, cullSettings);
}

void SceneCull::InitBarriers()
{
    m_ComputeWRBarrierBase = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Compute,
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};

    m_ComputeRWBarrierBase = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Compute,
        .BufferSourceMask = VK_ACCESS_SHADER_READ_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_WRITE_BIT};

    m_IndirectWRBarrierBase = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Compute,
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
}

void SceneCull::InitMeshCull(DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshCull.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/mesh-cull-comp.shader"}, "mesh-cull",
        allocator, layoutCache);

    m_MeshCull.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshCull.Template)
        .Build();

    m_MeshCullReocclusion.Template = m_MeshCull.Template;
    m_MeshCullReocclusion.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshCull.Template)
        .AddSpecialization("REOCCLUSION", true)
        .Build();
}

void SceneCull::InitMeshletCull(DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshletCull.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/meshlet-cull-comp.shader"}, "meshlet-cull",
        allocator, layoutCache);

    m_MeshletCull.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCull.Template)
        .Build();

    m_MeshletCullReocclusion.Template = m_MeshletCull.Template;

    m_MeshletCullReocclusion.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCullReocclusion.Template)
        .AddSpecialization("REOCCLUSION", true)
        .Build();

    m_MeshletCullClear.Template = ShaderTemplateLibrary::LoadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/clear-buffers-comp.shader"}, "clear-buffers-cull",
        allocator, layoutCache);

    m_MeshletCullClear.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCullClear.Template)
        .Build();

    m_MeshletCullClear.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCullClear.Template)
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleMeshletCount(), sizeof(u32), 0)
        .Build();
}
