#pragma once
#include <glm/fwd.hpp>

#include "Core/Camera.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Shader.h"

struct FrameContext;
class ResourceUploader;
class DepthPyramid;
class Scene;

class SceneCullBuffers
{
public:
    struct SceneCullData
    {
        glm::mat4 ViewMatrix;
        FrustumPlanes FrustumPlanes;
        ProjectionData ProjectionData;
        f32 PyramidWidth;
        f32 PyramidHeight;
        u32 Pad0;
        u32 Pad1;
    };
    struct SceneCullDataExtended
    {
        glm::mat4 ViewProjectionMatrix;
        FrustumPlanes FrustumPlanes;
        ProjectionData ProjectionData;
        f32 PyramidWidth;
        f32 PyramidHeight;
        u32 Pad0;
        u32 Pad1;
    };

public:
    void Init();
    void Update(const Camera& camera, const DepthPyramid* depthPyramid,  ResourceUploader& resourceUploader, const FrameContext& frameContext);

    const Buffer& GetCullData() const { return m_CullDataUBO.Buffer; }
    const Buffer& GetCullDataExtended() const { return m_CullDataUBOExtended.Buffer; }
    
    const Buffer& GetVisibleMeshCount() const { return m_CountBuffers.VisibleMeshes; }
    const Buffer& GetVisibleMeshletCount() const { return m_CountBuffers.VisibleMeshlets; }
    const Buffer& GetOccludedMeshletCount() const { return m_CountBuffers.OccludedMeshlets; }
    const Buffer& GetOccludedTriangleCounts() const { return m_CountBuffers.OccludedTriangles; }
    
    const Buffer& GetIndirectDispatch() const { return m_IndirectDispatch; }

    const Buffer& GetUncompactedCommands() const { return m_UncompactedCommands; }
    const Buffer& GetUncompactedCommandCount() const { return m_UncompactedCommandCount; }
private:
    struct SceneCullDataUBO
    {
        SceneCullData SceneData;
        Buffer Buffer;
    };
    struct SceneCullDataUBOExtended
    {
        SceneCullDataExtended SceneData;
        Buffer Buffer;
    };
    struct CountBuffers
    {
        Buffer VisibleMeshes;
        Buffer VisibleMeshlets;
        Buffer OccludedMeshlets;
        Buffer OccludedTriangles;
    };

    SceneCullDataUBO m_CullDataUBO{};
    SceneCullDataUBOExtended m_CullDataUBOExtended{};
    CountBuffers m_CountBuffers{};

    Buffer m_UncompactedCommands{};
    Buffer m_UncompactedCommandCount{};

    Buffer m_IndirectDispatch{};
};

class SceneCull
{
public:
    void Init(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void ShutDown();

    void SetDepthPyramid(const DepthPyramid& depthPyramid);
    void UpdateBuffers(const Camera& camera, ResourceUploader& resourceUploader, const FrameContext& frameContext);
    void ResetCullBuffers(const FrameContext& frameContext);
    void ResetSecondaryCullBuffers(const FrameContext& frameContext, u32 clearIndex);
    
    void PerformMeshCulling(const FrameContext& frameContext);
    void PerformMeshletCulling(const FrameContext& frameContext);
    void PerformMeshCompaction(const FrameContext& frameContext);
    void PerformMeshletCompaction(const FrameContext& frameContext);

    void ClearTriangleCullCommandBuffer(const FrameContext& frameContext);
    void ClearTriangleCullCommandBufferSecondary(const FrameContext& frameContext);
    void PerformTriangleCullingCompaction(const FrameContext& frameContext);
    void PerformSecondaryTriangleCullingCompaction(const FrameContext& frameContext);
    void PerformTertiaryTriangleCullingCompaction(const FrameContext& frameContext);

    void PerformSecondaryMeshCulling(const FrameContext& frameContext);
    void PerformSecondaryMeshletCulling(const FrameContext& frameContext);
    void PerformSecondaryMeshCompaction(const FrameContext& frameContext);
    void PerformSecondaryMeshletCompaction(const FrameContext& frameContext);

    void PerformFinalCompaction(const FrameContext& frameContext);

    const Buffer& GetVisibleCo  untBuffer() const;

    const SceneCullBuffers& GetSceneCullBuffers() const { return m_SceneCullBuffers; }
private:
    void DestroyDescriptors();
    
    void PerformIndirectDispatchBufferPrepare(const FrameContext& frameContext, u32 localGroupSize, u32 multiplier, u32 bufferIndex);

    void InitClearBuffers(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    
    void InitMeshCull(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitMeshletCull(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitMeshCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitMeshletCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitPrepareIndirectDispatch(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);

    void InitMeshCullSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitMeshletCullSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitMeshCompactSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitMeshletCompactSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);

    void InitTriangleClearCullCommands(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitTriangleCullCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitTriangleCullCompactSecondary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitTriangleCullCompactTertiary(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);

    void InitFinalIndirectBufferCompact(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
private:
    Scene* m_Scene{nullptr};
    SceneCullBuffers m_SceneCullBuffers{};

    struct ComputePipelineData
    {
        ShaderPipelineTemplate* Template;
        ShaderPipeline Pipeline;
        ShaderDescriptorSet DescriptorSet;
    };
    
    ComputePipelineData m_ClearBuffersData{};
    ComputePipelineData m_ClearSecondaryBufferData{};
    
    ComputePipelineData m_MeshCullData{};
    ComputePipelineData m_MeshletCullData{};
    ComputePipelineData m_MeshletCompactData{};
    ComputePipelineData m_MeshCompactVisibleData{};

    ComputePipelineData m_PrepareIndirectDispatch{};

    ComputePipelineData m_MeshCullSecondaryData{};
    ComputePipelineData m_MeshletCullSecondaryData{};
    ComputePipelineData m_MeshCompactVisibleSecondaryData{};
    ComputePipelineData m_MeshletCompactSecondaryData{};

    ComputePipelineData m_ClearTriangleCullCommandsData{};
    ComputePipelineData m_ClearTriangleCullCommandsSecondaryData{};
    ComputePipelineData m_TriangleCullCompactData{};
    ComputePipelineData m_TriangleCullCompactSecondaryData{};
    ComputePipelineData m_TriangleCullCompactTertiaryData{};

    ComputePipelineData m_CompactFinalIndirectBufferData{};
    
    const DepthPyramid* m_DepthPyramid{nullptr};
    bool m_CullIsInitialized{false};
};
