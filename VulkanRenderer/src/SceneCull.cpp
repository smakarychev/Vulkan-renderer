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

    Buffer::Builder ssboBuilder = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage})
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    Buffer::Builder ssboIndirectBuilder = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(sizeof(IndirectCommand) * MAX_DRAW_INDIRECT_CALLS)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    
    m_CullDataUBO.Buffer = uboBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<SceneCullData>(BUFFERED_FRAMES))
        .Build();
    m_CullDataUBOExtended.Buffer = uboBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<SceneCullDataExtended>(BUFFERED_FRAMES))
        .Build();
    
    m_CountBuffers.VisibleMeshlets = ssboBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<u32>(BUFFERED_FRAMES))
        .Build();
    m_CountBuffers.OccludedMeshlets = ssboBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<u32>(BUFFERED_FRAMES))
        .Build();
    m_CountBuffers.VisibleMeshes = ssboBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<u32>(BUFFERED_FRAMES))
        .Build();

    m_CountBuffers.OccludedTriangles = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(sizeof(u32) * MAX_DRAW_INDIRECT_CALLS)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_IndirectDispatch = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<VkDispatchIndirectCommand>(BUFFERED_FRAMES))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_UncompactedCommands = ssboIndirectBuilder.Build();

    m_UncompactedCommandCount = ssboBuilder
        .SetSizeBytes(vkUtils::alignUniformBufferSizeBytes<u32>(BUFFERED_FRAMES))
        .Build();
}

void SceneCullBuffers::Update(const Camera& camera, const DepthPyramid* depthPyramid,  ResourceUploader& resourceUploader, const FrameContext& frameContext)
{
    auto& sceneCullData = m_CullDataUBO.SceneData;
    auto& sceneCullDataExtended = m_CullDataUBOExtended.SceneData;
     bool once = true;
    if (once)
    {
        sceneCullData.FrustumPlanes = camera.GetFrustumPlanes();
        sceneCullData.ProjectionData = camera.GetProjectionData();
        sceneCullData.ViewMatrix = camera.GetView();

        sceneCullDataExtended.FrustumPlanes = sceneCullData.FrustumPlanes;
        sceneCullDataExtended.ProjectionData = sceneCullData.ProjectionData;
        sceneCullDataExtended.ViewProjectionMatrix = camera.GetViewProjection();
        
        once = false;    
    }
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

void SceneCull::Init(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_Scene = &scene;
    m_SceneCullBuffers.Init();

    InitBarriers();
    
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
    
    InitTriangleClearCullCommands(scene, allocator, layoutCache);
    InitTriangleCullCompact(scene, allocator, layoutCache);
    InitTriangleCullCompactSecondary(scene, allocator, layoutCache);
    InitTriangleCullCompactTertiary(scene, allocator, layoutCache);

    InitFinalIndirectBufferCompact(scene, allocator, layoutCache);
}

void SceneCull::ShutDown()
{
    if (m_CullIsInitialized)
    {
        DestroyDescriptors();
    }
}

void SceneCull::DestroyDescriptors()
{
    ShaderDescriptorSet::Destroy(m_MeshletCullData.DescriptorSet);
    ShaderDescriptorSet::Destroy(m_MeshCullData.DescriptorSet);

    ShaderDescriptorSet::Destroy(m_MeshCullSecondaryData.DescriptorSet);
    ShaderDescriptorSet::Destroy(m_MeshletCullSecondaryData.DescriptorSet);

    ShaderDescriptorSet::Destroy(m_TriangleCullCompactData.DescriptorSet);
    ShaderDescriptorSet::Destroy(m_TriangleCullCompactSecondaryData.DescriptorSet);
    ShaderDescriptorSet::Destroy(m_TriangleCullCompactTertiaryData.DescriptorSet);
}

void SceneCull::SetDepthPyramid(const DepthPyramid& depthPyramid)
{
    m_DepthPyramid = &depthPyramid;

    if (m_CullIsInitialized)
        DestroyDescriptors();
    else
        m_CullIsInitialized = true;
    
    m_MeshCullData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCullData.Template)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_object_visibility_buffer", m_Scene->GetRenderObjectsVisibilityBuffer())
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullData(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();
    
    m_MeshletCullData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCullData.Template)
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullData(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_meshlet_buffer", m_Scene->GetMeshletsBuffer())
        .AddBinding("u_command_buffer", m_SceneCullBuffers.GetUncompactedCommands())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleMeshCount(), sizeof(u32), 0)
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();
    
    m_MeshCullSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCullData.Template)
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullData(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_object_visibility_buffer", m_Scene->GetRenderObjectsVisibilityBuffer())
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();

    m_MeshletCullSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCullSecondaryData.Template)
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullData(), sizeof(SceneCullBuffers::SceneCullData), 0)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_meshlet_buffer", m_Scene->GetMeshletsBuffer())
        .AddBinding("u_command_buffer", m_Scene->GetMeshletsIndirectFinalBuffer())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetOccludedMeshletCount(), sizeof(u32), 0)
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();

    m_TriangleCullCompactData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_TriangleCullCompactData.Template)
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullDataExtended(), sizeof(SceneCullBuffers::SceneCullDataExtended), 0)
        .AddBinding("u_command_buffer", m_Scene->GetMeshletsIndirectFinalBuffer())
        .AddBinding("u_final_command_buffer", m_SceneCullBuffers.GetUncompactedCommands())
        .AddBinding("u_command_count_buffer", m_SceneCullBuffers.GetUncompactedCommandCount(), sizeof(u32), 0)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_positions_buffer", m_Scene->GetPositionsBuffer())
        .AddBinding("u_indices_buffer", m_Scene->GetIndicesBuffer())
        .AddBinding("u_culled_indices_buffer", m_Scene->GetIndicesCompactBuffer())
        .AddBinding("u_occluded_triangle_count_buffer", m_SceneCullBuffers.GetOccludedTriangleCounts())
        .AddBinding("u_triangle_buffer", m_Scene->GetTrianglesCompactBuffer())
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();

    m_TriangleCullCompactSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_TriangleCullCompactSecondaryData.Template)
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullDataExtended(), sizeof(SceneCullBuffers::SceneCullDataExtended), 0)
        .AddBinding("u_final_command_buffer", m_SceneCullBuffers.GetUncompactedCommands())
        .AddBinding("u_command_count_buffer", m_SceneCullBuffers.GetUncompactedCommandCount(), sizeof(u32), 0)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_positions_buffer", m_Scene->GetPositionsBuffer())
        .AddBinding("u_culled_indices_buffer", m_Scene->GetIndicesCompactBuffer())
        .AddBinding("u_occluded_triangle_count_buffer", m_SceneCullBuffers.GetOccludedTriangleCounts())
        .AddBinding("u_triangle_buffer", m_Scene->GetTrianglesCompactBuffer())
        .AddBinding("u_depth_pyramid", {
            .View = depthPyramid.GetTexture().GetImageData().View,
            .Sampler = depthPyramid.GetSampler(),
            .Layout = VK_IMAGE_LAYOUT_GENERAL})
        .BuildManualLifetime();

    m_TriangleCullCompactTertiaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_TriangleCullCompactTertiaryData.Template)
        .AddBinding("u_scene_data", m_SceneCullBuffers.GetCullDataExtended(), sizeof(SceneCullBuffers::SceneCullDataExtended), 0)
        .AddBinding("u_command_buffer", m_Scene->GetMeshletsIndirectFinalBuffer())
        .AddBinding("u_final_command_buffer", m_SceneCullBuffers.GetUncompactedCommands())
        .AddBinding("u_command_count_buffer", m_SceneCullBuffers.GetUncompactedCommandCount(), sizeof(u32), 0)
        .AddBinding("u_object_buffer", m_Scene->GetRenderObjectsBuffer())
        .AddBinding("u_positions_buffer", m_Scene->GetPositionsBuffer())
        .AddBinding("u_indices_buffer", m_Scene->GetIndicesBuffer())
        .AddBinding("u_culled_indices_buffer", m_Scene->GetIndicesCompactBuffer())
        .AddBinding("u_triangle_buffer", m_Scene->GetTrianglesCompactBuffer())
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

    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;

    m_ClearBuffersData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_ClearBuffersData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_ClearBuffersData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {countOffset, countOffset, countOffset, countOffset});
    RenderCommand::Dispatch(cmd, {1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetVisibleMeshCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommandCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
    
    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetVisibleMeshletCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetOccludedMeshletCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::ResetSecondaryCullBuffers(const FrameContext& frameContext, u32 clearIndex)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;

    PushConstantDescription pushConstantDescription = m_ClearSecondaryBufferData.Pipeline.GetPushConstantDescription();
    
    m_ClearSecondaryBufferData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_ClearSecondaryBufferData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_ClearSecondaryBufferData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {countOffset, countOffset});
    RenderCommand::PushConstants(cmd, m_ClearSecondaryBufferData.Pipeline.GetPipelineLayout(), &clearIndex, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetVisibleMeshletCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommandCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CullMeshes(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;

    u32 count = (u32)m_Scene->GetRenderObjects().size();
    PushConstantDescription pushConstantDescription = m_MeshCullData.Pipeline.GetPushConstantDescription();
    
    m_MeshCullData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshCullData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshCullData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {sceneCullOffset});
    RenderCommand::PushConstants(cmd, m_MeshCullData.Pipeline.GetPipelineLayout(), &count, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {count / 64 + 1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetRenderObjectsVisibilityBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CullMeshlets(const FrameContext& frameContext)
{
    PerformIndirectDispatchBufferPrepare(frameContext, 64, 1, 0);
    
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    
    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    
    u64 dispatchIndirectOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;
    
    m_MeshletCullData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshletCullData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshletCullData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {sceneCullOffset, countOffset});
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatch(), dispatchIndirectOffset);

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetMeshletsBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
    
    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CompactMeshes(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    
    u32 objectsCount = m_Scene->GetMeshletCount();
    PushConstantDescription pushConstantDescription = m_MeshCompactVisibleData.Pipeline.GetPushConstantDescription();

    m_MeshCompactVisibleData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshCompactVisibleData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshCompactVisibleData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {countOffset});
    RenderCommand::PushConstants(cmd, m_MeshCompactVisibleData.Pipeline.GetPipelineLayout(), &objectsCount, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {objectsCount / 64 + 1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetVisibleMeshCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CompactMeshlets(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u64 dispatchIndirectOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;
    
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;

    u32 maxCommandBufferIndex = MAX_DRAW_INDIRECT_CALLS - 1;
    PushConstantDescription pushConstantDescription = m_MeshletCompactData.Pipeline.GetPushConstantDescription();
    
    m_MeshletCompactData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshletCompactData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshletCompactData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {countOffset, countOffset, countOffset});
    RenderCommand::PushConstants(cmd, m_MeshletCompactData.Pipeline.GetPipelineLayout(), &maxCommandBufferIndex, pushConstantDescription);
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatch(), dispatchIndirectOffset);

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetMeshletsIndirectFinalBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommandCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetOccludedMeshletCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CullMeshesSecondary(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;

    u32 count = (u32)m_Scene->GetRenderObjects().size();
    PushConstantDescription pushConstantDescription = m_MeshCullSecondaryData.Pipeline.GetPushConstantDescription();
    
    m_MeshCullSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshCullSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshCullSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {sceneCullOffset});
    RenderCommand::PushConstants(cmd, m_MeshCullSecondaryData.Pipeline.GetPipelineLayout(), &count, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {count / 64 + 1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetRenderObjectsVisibilityBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CullMeshletsSecondary(const FrameContext& frameContext)
{
    PerformIndirectDispatchBufferPrepare(frameContext, 64, 1, 2);
    
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    
    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullData)) * frameContext.FrameNumber;
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
        
    u64 dispatchIndirectOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;

    u32 maxCommandBufferIndex = MAX_DRAW_INDIRECT_CALLS - 1;
    PushConstantDescription pushConstantDescription = m_MeshletCullSecondaryData.Pipeline.GetPushConstantDescription();

    m_MeshletCullSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshletCullSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshletCullSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {sceneCullOffset, countOffset});
    RenderCommand::PushConstants(cmd, m_MeshletCullSecondaryData.Pipeline.GetPipelineLayout(), &maxCommandBufferIndex, pushConstantDescription);
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatch(), dispatchIndirectOffset);

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetMeshletsIndirectFinalBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CompactMeshesSecondary(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    
    u32 objectsCount = m_Scene->GetMeshletCount();
    u32 maxCommandBufferIndex = MAX_DRAW_INDIRECT_CALLS - 1;
    std::array<u32, 2> pushConstants = {objectsCount, maxCommandBufferIndex};
    PushConstantDescription pushConstantDescription = m_MeshCompactVisibleSecondaryData.Pipeline.GetPushConstantDescription();

    m_MeshCompactVisibleSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshCompactVisibleSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshCompactVisibleSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {countOffset});
    RenderCommand::PushConstants(cmd, m_MeshCompactVisibleSecondaryData.Pipeline.GetPipelineLayout(), pushConstants.data(), pushConstantDescription);
    RenderCommand::Dispatch(cmd, {objectsCount / 64 + 1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetMeshletsIndirectFinalBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetOccludedMeshletCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CompactMeshletsSecondary(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;

    u64 dispatchIndirectOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;

    u32 maxCommandBufferIndex = MAX_DRAW_INDIRECT_CALLS - 1;
    PushConstantDescription pushConstantDescription = m_MeshletCompactSecondaryData.Pipeline.GetPushConstantDescription();
    
    m_MeshletCompactSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_MeshletCompactSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_MeshletCompactSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {countOffset, countOffset});
    RenderCommand::PushConstants(cmd, m_MeshletCompactSecondaryData.Pipeline.GetPipelineLayout(), &maxCommandBufferIndex, pushConstantDescription);
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatch(), dispatchIndirectOffset);

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetMeshletsIndirectFinalBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommandCount();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::ClearTriangleBuffers(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 count = m_Scene->GetMeshletCount();
    PushConstantDescription pushConstantDescription = m_ClearTriangleCullCommandsData.Pipeline.GetPushConstantDescription();

    m_ClearTriangleCullCommandsData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_ClearTriangleCullCommandsData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_ClearTriangleCullCommandsData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
    RenderCommand::PushConstants(cmd, m_ClearTriangleCullCommandsData.Pipeline.GetPipelineLayout(), &count, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {count / 256 + 1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetOccludedTriangleCounts();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::ClearTriangleBuffersSecondary(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 count = m_Scene->GetMeshletCount();
    PushConstantDescription pushConstantDescription = m_ClearTriangleCullCommandsSecondaryData.Pipeline.GetPushConstantDescription();
    
    m_ClearTriangleCullCommandsSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_ClearTriangleCullCommandsSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_ClearTriangleCullCommandsSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
    RenderCommand::PushConstants(cmd, m_ClearTriangleCullCommandsSecondaryData.Pipeline.GetPipelineLayout(), &count, pushConstantDescription);
    RenderCommand::Dispatch(cmd, {count / 256 + 1, 1, 1});

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CullCompactTriangles(const FrameContext& frameContext)
{
    PerformIndirectDispatchBufferPrepare(frameContext, 1, 1, 1);
    
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullDataExtended)) * frameContext.FrameNumber;
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    
    u64 dispatchIndirectOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;

    glm::uvec2 resolution = frameContext.Resolution;
    PushConstantDescription pushConstantDescription = m_TriangleCullCompactData.Pipeline.GetPushConstantDescription();

    m_TriangleCullCompactData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_TriangleCullCompactData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_TriangleCullCompactData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {sceneCullOffset, countOffset});

    for (u32 i = 0; i < assetLib::ModelInfo::TRIANGLES_PER_MESHLET / Driver::GetSubgroupSize(); i++)
    {
        glm::uvec3 pushConstants = {resolution.x, resolution.y, i * Driver::GetSubgroupSize()};
        RenderCommand::PushConstants(cmd, m_TriangleCullCompactData.Pipeline.GetPipelineLayout(), &pushConstants, pushConstantDescription);
        RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatch(), dispatchIndirectOffset);
    }

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetIndicesCompactBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetOccludedTriangleCounts();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CullCompactTrianglesSecondary(const FrameContext& frameContext)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    PerformIndirectDispatchBufferPrepare(frameContext, 1, 1, 1);

    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullDataExtended)) * frameContext.FrameNumber;
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    
    u64 dispatchIndirectOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;

    PushConstantDescription pushConstantDescription = m_TriangleCullCompactSecondaryData.Pipeline.GetPushConstantDescription();
    m_TriangleCullCompactSecondaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_TriangleCullCompactSecondaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_TriangleCullCompactSecondaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {sceneCullOffset, countOffset});

    for (u32 i = 0; i < assetLib::ModelInfo::TRIANGLES_PER_MESHLET / Driver::GetSubgroupSize(); i++)
    {
        u32 pushConstants = i * Driver::GetSubgroupSize();
        RenderCommand::PushConstants(cmd, m_TriangleCullCompactSecondaryData.Pipeline.GetPipelineLayout(), &pushConstants, pushConstantDescription);
        RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatch(), dispatchIndirectOffset);
    }
    
    m_ComputeWRBarrierBase.Buffer =  &m_SceneCullBuffers.GetUncompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetIndicesCompactBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CullCompactTrianglesTertiary(const FrameContext& frameContext)
{
    PerformIndirectDispatchBufferPrepare(frameContext, 1, 1, 1);
    
    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 sceneCullOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(SceneCullBuffers::SceneCullDataExtended)) * frameContext.FrameNumber;
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    
    u64 dispatchIndirectOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;

    glm::uvec2 resolution = frameContext.Resolution;
    PushConstantDescription pushConstantDescription = m_TriangleCullCompactTertiaryData.Pipeline.GetPushConstantDescription();

    m_TriangleCullCompactTertiaryData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_TriangleCullCompactTertiaryData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_TriangleCullCompactTertiaryData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {sceneCullOffset, countOffset});

    for (u32 i = 0; i < assetLib::ModelInfo::TRIANGLES_PER_MESHLET / Driver::GetSubgroupSize(); i++)
    {
        glm::uvec3 pushConstants = {resolution.x, resolution.y, i * Driver::GetSubgroupSize()};
        RenderCommand::PushConstants(cmd, m_TriangleCullCompactTertiaryData.Pipeline.GetPipelineLayout(), &pushConstants, pushConstantDescription);
        RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatch(), dispatchIndirectOffset);
    }
    
    m_ComputeWRBarrierBase.Buffer = &m_SceneCullBuffers.GetUncompactedCommands();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);

    m_ComputeWRBarrierBase.Buffer = &m_Scene->GetIndicesCompactBuffer();
    RenderCommand::CreateBarrier(cmd, m_ComputeWRBarrierBase);
}

void SceneCull::CompactCommands(const FrameContext& frameContext)
{
    PerformIndirectDispatchBufferPrepare(frameContext, 64, 1, 1);

    const CommandBuffer& cmd = frameContext.CommandBuffer;

    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    
    u64 dispatchIndirectOffset = vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;

    m_CompactFinalIndirectBufferData.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_CompactFinalIndirectBufferData.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_CompactFinalIndirectBufferData.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {countOffset, countOffset});
    RenderCommand::DispatchIndirect(cmd, m_SceneCullBuffers.GetIndirectDispatch(), dispatchIndirectOffset);

    m_IndirectWRBarrierBase.Buffer = &m_Scene->GetMeshletsIndirectFinalBuffer();
    RenderCommand::CreateBarrier(cmd, m_IndirectWRBarrierBase);

    m_IndirectWRBarrierBase.Buffer = &m_SceneCullBuffers.GetVisibleMeshletCount();
    RenderCommand::CreateBarrier(cmd, m_IndirectWRBarrierBase);
}

const Buffer& SceneCull::GetDrawCount() const
{
    return m_SceneCullBuffers.GetVisibleMeshletCount();
}

void SceneCull::PerformIndirectDispatchBufferPrepare(const FrameContext& frameContext, u32 localGroupSize, u32 multiplier, u32 bufferIndex)
{
    const CommandBuffer& cmd = frameContext.CommandBuffer;
    
    u32 countOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(u32)) * frameContext.FrameNumber;
    u32 dispatchIndirectOffset = (u32)vkUtils::alignUniformBufferSizeBytes(sizeof(VkDispatchIndirectCommand)) * frameContext.FrameNumber;

    std::vector<u32> pushConstants = {localGroupSize, multiplier, bufferIndex};
    PushConstantDescription pushConstantDescription = m_PrepareIndirectDispatch.Pipeline.GetPushConstantDescription();
    
    m_PrepareIndirectDispatch.Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    m_PrepareIndirectDispatch.DescriptorSet.Bind(cmd, DescriptorKind::Global, m_PrepareIndirectDispatch.Pipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, {countOffset, countOffset, countOffset, dispatchIndirectOffset});
    RenderCommand::PushConstants(cmd, m_PrepareIndirectDispatch.Pipeline.GetPipelineLayout(), pushConstants.data(), pushConstantDescription);
    RenderCommand::Dispatch(cmd, {1, 1, 1});

    m_IndirectWRBarrierBase.Buffer = &m_SceneCullBuffers.GetIndirectDispatch();
    RenderCommand::CreateBarrier(cmd, m_IndirectWRBarrierBase);
}

void SceneCull::InitBarriers()
{
    m_ComputeWRBarrierBase = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_SHADER_READ_BIT};

    m_IndirectWRBarrierBase = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        .Queue = &Driver::GetDevice().GetQueues().Graphics,
        .BufferSourceMask = VK_ACCESS_SHADER_WRITE_BIT, .BufferDestinationMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT};
}

void SceneCull::InitClearBuffers(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_ClearBuffersData.Template = sceneUtils::loadShaderPipelineTemplate(
       {"../assets/shaders/processed/culling/compute-clear-cull-buffers-comp.shader"}, "compute-clear-cull-buffers",
       scene, allocator, layoutCache);
    
    m_ClearBuffersData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_ClearBuffersData.Template)
        .Build();

    m_ClearBuffersData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_ClearBuffersData.Template)
        .AddBinding("u_compact_meshlets", m_SceneCullBuffers.GetVisibleMeshCount(), sizeof(u32), 0)
        .AddBinding("u_visible_uncompacted_meshlets", m_SceneCullBuffers.GetUncompactedCommandCount(), sizeof(u32), 0)
        .AddBinding("u_visible_meshlets", m_SceneCullBuffers.GetVisibleMeshletCount(), sizeof(u32), 0)
        .AddBinding("u_occluded_meshlets", m_SceneCullBuffers.GetOccludedMeshletCount(), sizeof(u32), 0)
        .Build();

    m_ClearSecondaryBufferData.Template = sceneUtils::loadShaderPipelineTemplate(
       {"../assets/shaders/processed/culling/compute-clear-cull-secondary-buffers-comp.shader"}, "compute-clear-cull-secondary-buffers",
       scene, allocator, layoutCache);
    
    m_ClearSecondaryBufferData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_ClearSecondaryBufferData.Template)
        .Build();

    m_ClearSecondaryBufferData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_ClearSecondaryBufferData.Template)
        .AddBinding("u_visible_uncompacted_meshlets", m_SceneCullBuffers.GetUncompactedCommandCount(), sizeof(u32), 0)
        .AddBinding("u_visible_meshlets", m_SceneCullBuffers.GetVisibleMeshletCount(), sizeof(u32), 0)
        .Build();
}

void SceneCull::InitMeshCull(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshCullData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-cull-meshes-comp.shader"}, "compute-mesh-cull",
        scene, allocator, layoutCache);
    
    m_MeshCullData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshCullData.Template)
        .Build();
}

void SceneCull::InitMeshletCull(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshletCullData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-cull-meshlets-comp.shader"}, "compute-meshlet-cull",
        scene, allocator, layoutCache);
   
    m_MeshletCullData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCullData.Template)
        .Build();
}

void SceneCull::InitMeshCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshCompactVisibleData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-compact-visible-meshes-comp.shader"}, "compute-compact-visible-meshes",
        scene, allocator, layoutCache);
    
    m_MeshCompactVisibleData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshCompactVisibleData.Template)
        .Build();

    m_MeshCompactVisibleData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCompactVisibleData.Template)
        .AddBinding("u_command_buffer", scene.GetMeshletsIndirectBuffer())
        .AddBinding("u_compacted_command_buffer", m_SceneCullBuffers.GetUncompactedCommands())
        .AddBinding("u_object_visibility_buffer", scene.GetRenderObjectsVisibilityBuffer())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleMeshCount(), sizeof(u32), 0)
        .Build();
}

void SceneCull::InitMeshletCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshletCompactData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-compact-meshlets-comp.shader"}, "compute-compact-meshlets",
        scene, allocator, layoutCache);
    
    m_MeshletCompactData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCompactData.Template)
        .Build();

    m_MeshletCompactData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCompactData.Template)
        .AddBinding("u_command_buffer", m_SceneCullBuffers.GetUncompactedCommands())
        .AddBinding("u_compacted_command_buffer", scene.GetMeshletsIndirectFinalBuffer())
        .AddBinding("u_meshlet_buffer", scene.GetMeshletsBuffer())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleMeshCount(), sizeof(u32), 0)
        .AddBinding("u_occluded_buffer", m_SceneCullBuffers.GetOccludedMeshletCount(), sizeof(u32), 0)
        .AddBinding("u_visible_buffer", m_SceneCullBuffers.GetUncompactedCommandCount(), sizeof(u32), 0)
        .Build();
}

void SceneCull::InitPrepareIndirectDispatch(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_PrepareIndirectDispatch.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-prepare-dispatch-indirect-comp.shader"}, "compute-prepare-dispatch-indirect",
        scene, allocator, layoutCache);

    m_PrepareIndirectDispatch.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_PrepareIndirectDispatch.Template)
        .Build();

    m_PrepareIndirectDispatch.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_PrepareIndirectDispatch.Template)
        .AddBinding("u_occluded_buffer", m_SceneCullBuffers.GetOccludedMeshletCount(), sizeof(u32), 0)
        .AddBinding("u_visible_buffer", m_SceneCullBuffers.GetUncompactedCommandCount(), sizeof(u32), 0)
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetVisibleMeshCount(), sizeof(u32), 0)
        .AddBinding("u_indirect_dispatch_buffer", m_SceneCullBuffers.GetIndirectDispatch(), sizeof(VkDispatchIndirectCommand), 0)
        .Build();
}

void SceneCull::InitMeshCullSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshCullSecondaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-cull-meshes-secondary-comp.shader"}, "compute-mesh-cull-secondary",
        scene, allocator, layoutCache);

    m_MeshCullSecondaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshCullSecondaryData.Template)
        .Build();
}

void SceneCull::InitMeshletCullSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshletCullSecondaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-cull-meshlets-secondary-comp.shader"}, "compute-meshlet-cull-secondary",
        scene, allocator, layoutCache);

    m_MeshletCullSecondaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCullSecondaryData.Template)
        .Build();
}

void SceneCull::InitMeshCompactSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshCompactVisibleSecondaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-compact-visible-meshes-secondary-comp.shader"}, "compute-compact-visible-meshes-secondary",
        scene, allocator, layoutCache);
    
    m_MeshCompactVisibleSecondaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshCompactVisibleSecondaryData.Template)
        .Build();

    m_MeshCompactVisibleSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshCompactVisibleSecondaryData.Template)
        .AddBinding("u_command_buffer", scene.GetMeshletsIndirectBuffer())
        .AddBinding("u_compacted_command_buffer", scene.GetMeshletsIndirectFinalBuffer())
        .AddBinding("u_object_visibility_buffer", scene.GetRenderObjectsVisibilityBuffer())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetOccludedMeshletCount(), sizeof(u32), 0)
        .Build();
}

void SceneCull::InitMeshletCompactSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_MeshletCompactSecondaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-compact-meshlets-secondary-comp.shader"}, "compute-compact-meshlets-secondary",
        scene, allocator, layoutCache);
    
    m_MeshletCompactSecondaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_MeshletCompactSecondaryData.Template)
        .Build();

    m_MeshletCompactSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_MeshletCompactSecondaryData.Template)
        .AddBinding("u_command_buffer", m_Scene->GetMeshletsIndirectFinalBuffer())
        .AddBinding("u_count_buffer", m_SceneCullBuffers.GetOccludedMeshletCount(), sizeof(u32), 0)
        .AddBinding("u_visible_buffer", m_SceneCullBuffers.GetUncompactedCommandCount(), sizeof(u32), 0)
        .Build();
}

void SceneCull::InitTriangleClearCullCommands(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_ClearTriangleCullCommandsData.Template = sceneUtils::loadShaderPipelineTemplate(
       {"../assets/shaders/processed/culling/compute-clear-cull-triangles-commands-comp.shader"}, "compute-clear-triangle-cull-commands",
       scene, allocator, layoutCache);
    
    m_ClearTriangleCullCommandsData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_ClearTriangleCullCommandsData.Template)
        .Build();

    m_ClearTriangleCullCommandsData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_ClearTriangleCullCommandsData.Template)
        .AddBinding("u_command_buffer", m_SceneCullBuffers.GetUncompactedCommands())
        .AddBinding("u_occluded_triangle_count_buffer", m_SceneCullBuffers.GetOccludedTriangleCounts())
        .Build();
    
    m_ClearTriangleCullCommandsSecondaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-clear-cull-triangles-commands-secondary-comp.shader"}, "compute-clear-triangle-cull-commands-secondary",
        scene, allocator, layoutCache);
    
    m_ClearTriangleCullCommandsSecondaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_ClearTriangleCullCommandsSecondaryData.Template)
        .Build();

    m_ClearTriangleCullCommandsSecondaryData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_ClearTriangleCullCommandsSecondaryData.Template)
        .AddBinding("u_command_buffer", m_SceneCullBuffers.GetUncompactedCommands())
        .Build();
}

void SceneCull::InitTriangleCullCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    std::string_view shaderPath = Driver::GetSubgroupSize() == 64 ?
        "../assets/shaders/processed/culling/amd/compute-cull-compact-triangles-comp.shader" :
        "../assets/shaders/processed/culling/nvidia/compute-cull-compact-triangles-comp.shader";
    m_TriangleCullCompactData.Template = sceneUtils::loadShaderPipelineTemplate(
        {shaderPath}, "compute-cull-compact-triangles",
        scene, allocator, layoutCache);

    m_TriangleCullCompactData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_TriangleCullCompactData.Template)
        .Build();
}

void SceneCull::InitTriangleCullCompactSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    std::string_view shaderPath = Driver::GetSubgroupSize() == 64 ?
        "../assets/shaders/processed/culling/amd/compute-cull-compact-triangles-secondary-comp.shader" :
        "../assets/shaders/processed/culling/nvidia/compute-cull-compact-triangles-secondary-comp.shader";
    m_TriangleCullCompactSecondaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {shaderPath}, "compute-cull-compact-triangles-secondary",
        scene, allocator, layoutCache);

    m_TriangleCullCompactSecondaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_TriangleCullCompactSecondaryData.Template)
        .Build();
}

void SceneCull::InitTriangleCullCompactTertiary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    std::string_view shaderPath = Driver::GetSubgroupSize() == 64 ?
        "../assets/shaders/processed/culling/amd/compute-cull-compact-triangles-tertiary-comp.shader" :
        "../assets/shaders/processed/culling/nvidia/compute-cull-compact-triangles-tertiary-comp.shader";
    m_TriangleCullCompactTertiaryData.Template = sceneUtils::loadShaderPipelineTemplate(
        {shaderPath}, "compute-cull-compact-triangles-tertiary",
        scene, allocator, layoutCache);

    m_TriangleCullCompactTertiaryData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_TriangleCullCompactTertiaryData.Template)
        .Build();
}

void SceneCull::InitFinalIndirectBufferCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache)
{
    m_CompactFinalIndirectBufferData.Template = sceneUtils::loadShaderPipelineTemplate(
        {"../assets/shaders/processed/culling/compute-compact-final-indirect-buffer-comp.shader"}, "compute-compact-final-indirect-buffer",
        scene, allocator, layoutCache);

    m_CompactFinalIndirectBufferData.Pipeline = ShaderPipeline::Builder()
        .SetTemplate(m_CompactFinalIndirectBufferData.Template)
        .Build();

    m_CompactFinalIndirectBufferData.DescriptorSet = ShaderDescriptorSet::Builder()
        .SetTemplate(m_CompactFinalIndirectBufferData.Template)
        .AddBinding("u_final_command_buffer", m_SceneCullBuffers.GetUncompactedCommands())
        .AddBinding("u_final_command_compact_buffer", m_Scene->GetMeshletsIndirectFinalBuffer())
        .AddBinding("u_final_command_count_buffer", m_SceneCullBuffers.GetUncompactedCommandCount(), sizeof(u32), 0)
        .AddBinding("u_final_compact_command_count_buffer", m_SceneCullBuffers.GetVisibleMeshletCount(), sizeof(u32), 0)
        .Build();
}
