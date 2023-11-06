#include "SceneCull.h"

#include "Renderer.h"
#include "ResourceUploader.h"
#include "Scene.h"
#include "Settings.h"
#include "utils/SceneUtils.h"
#include "Vulkan/DepthPyramid.h"
#include "Vulkan/VulkanUtils.h"

void SceneCullBuffers::Init()
{
    Buffer::Builder uboBuilder = Buffer::Builder()
        .SetKind({BufferKind::Uniform})
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    Buffer::Builder ssboHostBuilder = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage})
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    Buffer::Builder ssboIndirectBuilder = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(sizeof(VkDrawIndexedIndirectCommand) * MAX_DRAW_INDIRECT_CALLS)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    
    m_CullDataUBO.Buffer = uboBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<SceneCullData>(BUFFERED_FRAMES))
        .Build();
    m_CompactOccludeBuffers.VisibleCountBuffer = ssboHostBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<CompactMeshletData>(BUFFERED_FRAMES))
        .Build();
    m_CompactOccludeBuffers.VisibleCountSecondaryBuffer = ssboHostBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<CompactMeshletData>(BUFFERED_FRAMES))
        .Build();
    m_CompactOccludeBuffers.OccludedCountBuffer = ssboHostBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<OccludeMeshletData>(BUFFERED_FRAMES))
        .Build();
    m_CompactRenderObjectSSBO.Buffer = ssboHostBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<CompactRenderObjectData>(BUFFERED_FRAMES))
        .Build();

    m_CompactOccludeBuffers.IndirectOccludedMeshletBuffer = ssboIndirectBuilder.Build();
    m_CompactOccludeBuffers.IndirectVisibleRenderObjectBuffer = ssboIndirectBuilder.Build();

    m_IndirectDispatchBuffer = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(BUFFERED_FRAMES))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();
}

void SceneCullBuffers::Update(const Camera& camera, const DepthPyramid* depthPyramid,  ResourceUploader& resourceUploader, const FrameContext& frameContext)
{
    auto& sceneCullData = m_CullDataUBO.SceneData;
     bool once = true;
    if (once)
    {
        sceneCullData.FrustumPlanes = camera.GetFrustumPlanes();
        sceneCullData.ProjectionData = camera.GetProjectionData();
        sceneCullData.ViewMatrix = camera.GetView();
        once = false;    
    }
    if (depthPyramid)
    {
        sceneCullData.PyramidWidth = (f32)depthPyramid->GetTexture().GetImageData().Width;
        sceneCullData.PyramidHeight = (f32)depthPyramid->GetTexture().GetImageData().Height;
    }
    resourceUploader.UpdateBuffer(m_CullDataUBO.Buffer, &sceneCullData,
        sizeof(sceneCullData), vkUtils::alignUniformBufferSizeBytes(sizeof(sceneCullData)) * frameContext.FrameNumber);
}

void SceneCull::Init(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_Scene = &scene;
    m_SceneCullBuffers.Init();

    InitClearBuffers(scene, allocator, layoutCache);

    InitMeshCull(scene, allocator, layoutCache);
    InitMeshletCull(scene, allocator, layoutCache);
    InitMeshCompact(scene, allocator, layoutCache);
    InitMeshletCompact(scene, allocator, layoutCache);
    InitPrepareIndirectDispatch(scene, allocator, layoutCache);

    InitMeshCullSecondary(scene, allocator, layoutCache);
    InitMeshletCullSecondary(scene, allocator, layoutCache);
    InitMeshCompactSecondary(scene, allocator, layoutCache);
    InitMeshletCompactSecondary(scene, allocator, layoutCache);
}

void SceneCull::ShutDown()
{
    if (m_CullIsInitialized)
    {
        ShaderDescriptorSet::Destroy(m_MeshletCullData.DescriptorSet);
        ShaderDescriptorSet::Destroy(m_MeshCullData.DescriptorSet);

        ShaderDescriptorSet::Destroy(m_MeshCullSecondaryData.DescriptorSet);
        ShaderDescriptorSet::Destroy(m_MeshletCullSecondaryData.DescriptorSet);
    }
}

void SceneCull::SetDepthPyramid(const DepthPyramid& depthPyramid)
{
    m_DepthPyramid = &depthPyramid;

    if (m_CullIsInitialized)
    {
        ShaderDescriptorSet::Destroy(m_MeshletCullData.DescriptorSet);
        ShaderDescriptorSet::Destroy(m_MeshCullData.DescriptorSet);
    }
    else
    {
        m_CullIsInitialized = true;
    }
    
    m_MeshCullData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCullData.Template)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_object_visibility_buffer", m_Scene->GetRenderObjectsVisibilityBuffer())
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullDataBuffer(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();
    
    m_MeshletCullData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCullData.Template)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_meshlet_buffer", m_Scene->GetMeshletsBuffer())
        .AddBinding("u_command_buffer", m_SceneCullBuffers.GetIndirectVisibleRenderObjectBuffer())
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullDataBuffer(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleRenderObjectCountBuffer(), sizeof(SceneCullBuffers::CompactRenderObjectData), 0)
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();

    
    m_MeshCullSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCullData.Template)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_object_visibility_buffer", m_Scene->GetRenderObjectsVisibilityBuffer())
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullDataBuffer(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();

    m_MeshletCullSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCullSecondaryData.Template)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_meshlet_buffer", m_Scene->GetMeshletsBuffer())
        .AddBinding("u_command_buffer", m_SceneCullBuffers.GetIndirectOccludedMeshletBuffer())
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullDataBuffer(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetOccludedCountBuffer(), sizeof(SceneCullBuffers::OccludeMeshletData), 0)
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();
}

void SceneCull::UpdateBuffers(const Camera& camera, ResourceUploader& resourceUploader, const FrameContext& frameContext)
{
    m_SceneCullBuffers.Update(camera, m_DepthPyramid, resourceUploader, frameContext);
}

void SceneCull::ResetCullBuffers(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    u32 offset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;

    m_ClearBuffersData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_ClearBuffersData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_ClearBuffersData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {offset, offset, offset, offset});
    RenderCommand::Dispatch(cmd, {1, 1, 1});
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_SceneCullBuffers.GetVisibleRenderObjectCountBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);

    barrierInfo.Buffer = &m_SceneCullBuffers.GetVisibleCountBuffer();
    RenderCommand::CreateBarrier(cmd, barrierInfo);

    barrierInfo.Buffer = &m_SceneCullBuffers.GetVisibleCountSecondaryBuffer();
    RenderCommand::CreateBarrier(cmd, barrierInfo);

    barrierInfo.Buffer = &m_SceneCullBuffers.GetOccludedCountBuffer();
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void SceneCull::PerformMeshCulling(const FrameContext& frameContext)
{
    m_CullStage = CullStage::Primary;
    
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    u32 offset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;

    u32 objectsCount = (u32)m_Scene->GetRenderObjects().size();
    PushConstantDescription pushConstantDescription = m_MeshCullData.Pipeline.GetPushConstantDescription();
    
    m_MeshCullData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshCullData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshCullData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {offset});
    RenderCommand::PushConstants(cmd, m_MeshCullData.Pipeline.GetPipelineLayout(), &objectsCount, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {objectsCount / 64 + 1, 1, 1});
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_Scene->GetRenderObjectsVisibilityBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void SceneCull::PerformMeshletCulling(const FrameContext& frameContext)
{
    PerformIndirectDispatchBufferPrepare(frameContext, 64, 0);
    
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    
    u64 indirectDispatchOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;
        
    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;
    u32 compactCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::CompactRenderObjectData)) * frameContext.FrameNumber;

    m_MeshletCullData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshletCullData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshletCullData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {sceneCullOffset, compactCountOffset});
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatchBuffer(), indirectDispatchOffset);
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_SceneCullBuffers.GetIndirectVisibleRenderObjectBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void SceneCull::PerformMeshCompaction(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    
    u32 compactCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::CompactRenderObjectData)) * frameContext.FrameNumber;
    
    u32 objectsCount = m_Scene->GetMeshletCount();
    PushConstantDescription pushConstantDescription = m_MeshCompactVisibleData.Pipeline.GetPushConstantDescription();

    m_MeshCompactVisibleData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshCompactVisibleData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshCompactVisibleData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {compactCountOffset});
    RenderCommand::PushConstants(cmd, m_MeshCompactVisibleData.Pipeline.GetPipelineLayout(), &objectsCount, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {objectsCount / 64 + 1, 1, 1});

    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_SceneCullBuffers.GetIndirectVisibleRenderObjectBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);

    barrierInfo.Buffer = &m_SceneCullBuffers.GetVisibleRenderObjectCountBuffer();
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void SceneCull::PerformMeshletCompaction(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u64 indirectDispatchOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;
    
    u32 compactCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::CompactRenderObjectData)) * frameContext.FrameNumber;
    u32 compactVisibleCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::CompactMeshletData)) * frameContext.FrameNumber;
    u32 compactOccludeCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::OccludeMeshletData)) * frameContext.FrameNumber;

    m_MeshletCompactData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshletCompactData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshletCompactData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {compactCountOffset, compactVisibleCountOffset, compactOccludeCountOffset});
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatchBuffer(), indirectDispatchOffset);
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_Scene->GetMeshletsIndirectFinalBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);

    barrierInfo.Buffer = &m_SceneCullBuffers.GetVisibleCountBuffer();
    RenderCommand::CreateBarrier(cmd, barrierInfo);

    barrierInfo.Buffer = &m_SceneCullBuffers.GetIndirectOccludedMeshletBuffer();
    RenderCommand::CreateBarrier(cmd, barrierInfo);

    barrierInfo.Buffer = &m_SceneCullBuffers.GetOccludedCountBuffer();
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void SceneCull::PerformSecondaryMeshCulling(const FrameContext& frameContext)
{
    m_CullStage = CullStage::Secondary;
    
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    u32 offset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;

    u32 objectsCount = (u32)m_Scene->GetRenderObjects().size();
    PushConstantDescription pushConstantDescription = m_MeshCullSecondaryData.Pipeline.GetPushConstantDescription();
    
    m_MeshCullSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshCullSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshCullSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {offset});
    RenderCommand::PushConstants(cmd, m_MeshCullSecondaryData.Pipeline.GetPipelineLayout(), &objectsCount, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {objectsCount / 64 + 1, 1, 1});
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_Scene->GetRenderObjectsVisibilityBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void SceneCull::PerformSecondaryMeshletCulling(const FrameContext& frameContext)
{
    PerformIndirectDispatchBufferPrepare(frameContext, 64, 2);
    
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    
    u64 indirectDispatchOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;
        
    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;
    u32 compactCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::CompactRenderObjectData)) * frameContext.FrameNumber;

    m_MeshletCullSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshletCullSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshletCullSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {sceneCullOffset, compactCountOffset});
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatchBuffer(), indirectDispatchOffset);
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_SceneCullBuffers.GetIndirectOccludedMeshletBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void SceneCull::PerformSecondaryMeshCompaction(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    PipelineBufferBarrierInfo countBarrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_HOST_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_SceneCullBuffers.GetIndirectOccludedMeshletBuffer(),
        .BufferSourceMask = VK_ACCESS_HOST_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};
    RenderCommand::CreateBarrier(cmd, countBarrierInfo);
    
    u32 compactCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::OccludeMeshletData)) * frameContext.FrameNumber;
    
    u32 objectsCount = m_Scene->GetMeshletCount();
    PushConstantDescription pushConstantDescription = m_MeshCompactVisibleSecondaryData.Pipeline.GetPushConstantDescription();

    m_MeshCompactVisibleSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshCompactVisibleSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshCompactVisibleSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {compactCountOffset});
    RenderCommand::PushConstants(cmd, m_MeshCompactVisibleSecondaryData.Pipeline.GetPipelineLayout(), &objectsCount, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {objectsCount / 64 + 1, 1, 1});

    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_SceneCullBuffers.GetIndirectOccludedMeshletBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);

    barrierInfo.Buffer = &m_SceneCullBuffers.GetOccludedCountBuffer();
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void SceneCull::PerformSecondaryMeshletCompaction(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u64 indirectDispatchOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;
    
    u32 compactCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::CompactRenderObjectData)) * frameContext.FrameNumber;
    u32 compactVisibleCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::CompactMeshletData)) * frameContext.FrameNumber;

    m_MeshletCompactSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshletCompactSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshletCompactSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {compactCountOffset, compactVisibleCountOffset});
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatchBuffer(), indirectDispatchOffset);
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_Scene->GetMeshletsIndirectFinalBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);

    barrierInfo.Buffer = &m_SceneCullBuffers.GetVisibleCountSecondaryBuffer();
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

const Buffer& SceneCull::GetVisibleMeshletsBuffer() const
{
    if (m_CullStage == CullStage::Primary)
        return m_SceneCullBuffers.GetVisibleCountBuffer();

    return m_SceneCullBuffers.GetVisibleCountSecondaryBuffer();
}

void SceneCull::PerformIndirectDispatchBufferPrepare(const FrameContext& frameContext, u32 localGroupSize, u32 bufferIndex)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    u32 compactCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::CompactRenderObjectData)) * frameContext.FrameNumber;
    u32 compactVisibleCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::CompactMeshletData)) * frameContext.FrameNumber;
    u32 compactOccludeCountOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::OccludeMeshletData)) * frameContext.FrameNumber;
    u32 dispatchIndirectOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;

    std::vector<u32> pushConstants = {localGroupSize, bufferIndex};
    PushConstantDescription pushConstantDescription = m_PrepareIndirectDispatch.Pipeline.GetPushConstantDescription();
    
    m_PrepareIndirectDispatch.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_PrepareIndirectDispatch.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_PrepareIndirectDispatch.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {compactCountOffset, compactVisibleCountOffset, compactOccludeCountOffset, dispatchIndirectOffset});
    RenderCommand::PushConstants(cmd, m_PrepareIndirectDispatch.Pipeline.GetPipelineLayout(), pushConstants.data(), pushConstantDescription);
    RenderCommand::Dispatch(cmd, {1, 1, 1});
    PipelineBufferBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .Buffer = &m_SceneCullBuffers.GetIndirectDispatchBuffer(),
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);
}

void SceneCull::InitClearBuffers(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_ClearBuffersData.Template = sceneUtils::loadShaderPipelineTemplate(
       {"../assets/shaders/processed/compute-clear-cull-buffers-comp.shader"}, "compute-clear-cull-buffers",
       scene, allocator, layoutCache);
    
    m_ClearBuffersData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_ClearBuffersData.Template)
        .Build();

    m_ClearBuffersData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_ClearBuffersData.Template)
        .AddBinding("u_compact_meshlets", m_SceneCullBuffers.GetVisibleRenderObjectCountBuffer(), sizeof(u32), 0)
        .AddBinding("u_visible_meshlets", m_SceneCullBuffers.GetVisibleCountBuffer(), sizeof(u32), 0)
        .AddBinding("u_visible_secondary_meshlets", m_SceneCullBuffers.GetVisibleCountSecondaryBuffer(), sizeof(u32), 0)
        .AddBinding("u_occluded_meshlets", m_SceneCullBuffers.GetOccludedCountBuffer(), sizeof(u32), 0)
        .Build();
}

void SceneCull::InitMeshCull(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshCullData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/compute-cull-meshes-comp.shader"}, "compute-mesh-cull",
        scene, allocator, layoutCache);
    
    m_MeshCullData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshCullData.Template)
        .Build();
}

void SceneCull::InitMeshletCull(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshletCullData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/compute-cull-meshlets-comp.shader"}, "compute-meshlet-cull",
        scene, allocator, layoutCache);
   
    m_MeshletCullData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCullData.Template)
        .Build();
}

void SceneCull::InitMeshCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshCompactVisibleData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/compute-compact-visible-meshes-comp.shader"}, "compute-compact-visible-meshes",
        scene, allocator, layoutCache);
    
    m_MeshCompactVisibleData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshCompactVisibleData.Template)
        .Build();

    m_MeshCompactVisibleData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCompactVisibleData.Template)
        .AddBinding("u_command_buffer", scene.GetMeshletsIndirectBuffer())
        .AddBinding("u_compacted_command_buffer", m_SceneCullBuffers.GetIndirectVisibleRenderObjectBuffer())
        .AddBinding("u_meshlet_buffer", scene.GetMeshletsBuffer())
        .AddBinding("u_object_visibility_buffer", scene.GetRenderObjectsVisibilityBuffer())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleRenderObjectCountBuffer(), sizeof(SceneCullBuffers::CompactRenderObjectData), 0)
        .Build();
}

void SceneCull::InitMeshletCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshletCompactData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/compute-compact-meshlets-comp.shader"}, "compute-compact-meshlets",
        scene, allocator, layoutCache);
    
    m_MeshletCompactData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCompactData.Template)
        .Build();

    m_MeshletCompactData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCompactData.Template)
        .AddBinding("u_command_buffer", m_SceneCullBuffers.GetIndirectVisibleRenderObjectBuffer())
        .AddBinding("u_compacted_command_buffer", scene.GetMeshletsIndirectFinalBuffer())
        .AddBinding("u_compacted_occluded_command_buffer", m_SceneCullBuffers.GetIndirectOccludedMeshletBuffer())
        .AddBinding("u_meshlet_buffer", scene.GetMeshletsBuffer())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleRenderObjectCountBuffer(), sizeof(SceneCullBuffers::CompactRenderObjectData), 0)
        .AddBinding("u_occluded_buffer", m_SceneCullBuffers.GetOccludedCountBuffer(), sizeof(SceneCullBuffers::CompactMeshletData), 0)
        .AddBinding("u_visible_buffer", m_SceneCullBuffers.GetVisibleCountBuffer(), sizeof(SceneCullBuffers::OccludeMeshletData), 0)
        .Build();
}

void SceneCull::InitPrepareIndirectDispatch(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_PrepareIndirectDispatch.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/compute-prepare-dispatch-indirect-comp.shader"}, "compute-prepare-dispatch-indirect",
        scene, allocator, layoutCache);

    m_PrepareIndirectDispatch.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_PrepareIndirectDispatch.Template)
        .Build();

    m_PrepareIndirectDispatch.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_PrepareIndirectDispatch.Template)
        .AddBinding("u_occluded_buffer", m_SceneCullBuffers.GetOccludedCountBuffer(), sizeof(SceneCullBuffers::CompactMeshletData), 0)
        .AddBinding("u_visible_buffer", m_SceneCullBuffers.GetVisibleCountBuffer(), sizeof(SceneCullBuffers::OccludeMeshletData), 0)
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleRenderObjectCountBuffer(), sizeof(SceneCullBuffers::CompactRenderObjectData), 0)
        .AddBinding("u_indirect_dispatch_buffer", m_SceneCullBuffers.GetIndirectDispatchBuffer(), sizeof(VkDispatchIndirectCommand), 0)
        .Build();
}

void SceneCull::InitMeshCullSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshCullSecondaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/compute-cull-meshes-secondary-comp.shader"}, "compute-mesh-cull-secondary",
        scene, allocator, layoutCache);

    m_MeshCullSecondaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshCullSecondaryData.Template)
        .Build();
}

void SceneCull::InitMeshletCullSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshletCullSecondaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/compute-cull-meshlets-secondary-comp.shader"}, "compute-meshlet-cull-secondary",
        scene, allocator, layoutCache);

    m_MeshletCullSecondaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCullSecondaryData.Template)
        .Build();
}

void SceneCull::InitMeshCompactSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshCompactVisibleSecondaryData.Template = m_MeshCompactVisibleData.Template;
    
    m_MeshCompactVisibleSecondaryData.Pipeline = m_MeshCompactVisibleData.Pipeline;

    m_MeshCompactVisibleSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCompactVisibleSecondaryData.Template)
        .AddBinding("u_command_buffer", scene.GetMeshletsIndirectBuffer())
        .AddBinding("u_compacted_command_buffer", m_SceneCullBuffers.GetIndirectOccludedMeshletBuffer())
        .AddBinding("u_meshlet_buffer", scene.GetMeshletsBuffer())
        .AddBinding("u_object_visibility_buffer", scene.GetRenderObjectsVisibilityBuffer())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetOccludedCountBuffer(), sizeof(SceneCullBuffers::OccludeMeshletData), 0)
        .Build();
}

void SceneCull::InitMeshletCompactSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshletCompactSecondaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/compute-compact-meshlets-secondary-comp.shader"}, "compute-compact-meshlets-secondary",
        scene, allocator, layoutCache);
    
    m_MeshletCompactSecondaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCompactSecondaryData.Template)
        .Build();

    m_MeshletCompactSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCompactSecondaryData.Template)
        .AddBinding("u_command_buffer", m_SceneCullBuffers.GetIndirectOccludedMeshletBuffer())
        .AddBinding("u_compacted_command_buffer", scene.GetMeshletsIndirectFinalBuffer())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetOccludedCountBuffer(), sizeof(SceneCullBuffers::OccludeMeshletData), 0)
        .AddBinding("u_visible_buffer", m_SceneCullBuffers.GetVisibleCountSecondaryBuffer(), sizeof(SceneCullBuffers::CompactMeshletData), 0)
        .Build();
}
