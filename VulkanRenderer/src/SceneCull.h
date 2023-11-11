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
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewMatrix;
        FrustumPlanes FrustumPlanes;
        ProjectionData ProjectionData;
        f32 PyramidWidth;
        f32 PyramidHeight;
        u32 Pad0;
        u32 Pad1;
    };

    struct CompactMeshletData
    {
        u32 DrawCount;
    };
    struct CompactTriangleData
    {
        u32 Count;
    };
    struct OccludeMeshletData
    {
        u32 Count;
    };
    struct OccludeRenderObjectData
    {
        u32 Count;
    };
    struct CompactRenderObjectData
    {
        // count of indirect commands after mesh culling and compaction
        u32 DrawCount;
    };
public:
    void Init();
    void Update(const Camera& camera, const DepthPyramid* depthPyramid,  ResourceUploader& resourceUploader, const FrameContext& frameContext);

    const Buffer& GetCullDataBuffer() const { return m_CullDataUBO.Buffer; }
    const Buffer& GetCullDataExtendedBuffer() const { return m_CullDataUBOExtended.Buffer; }
    const Buffer& GetVisibleRenderObjectCountBuffer() const { return m_CompactRenderObjectSSBO.Buffer; }
    const Buffer& GetVisibleCountBuffer() const { return m_CompactOccludeBuffers.VisibleCountBuffer; }
    const Buffer& GetOccludedTriangleCountsBuffer() const { return m_CompactOccludeBuffers.OccludeTriangleCountsBuffer; }
    const Buffer& GetOccludedCountBuffer() const { return m_CompactOccludeBuffers.OccludedCountBuffer; }
    const Buffer& GetIndirectVisibleRenderObjectBuffer() const { return m_CompactOccludeBuffers.IndirectVisibleRenderObjectBuffer; }
    const Buffer& GetIndirectOccludedMeshletBuffer() const { return m_CompactOccludeBuffers.IndirectOccludedMeshletBuffer; }
    const Buffer& GetIndirectDispatchBuffer() const { return m_IndirectDispatchBuffer; }

    const Buffer& GetIndirectUncompactedBuffer() const { return m_IndirectUncompactedBuffer; }
    const Buffer& GetIndirectUncompactedCountBuffer() const { return m_IndirectUncompactedCountBuffer; }
    const Buffer& GetIndirectUncompactedOffsetBuffer() const { return m_IndirectUncompactedOffsetBuffer; }

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
    struct CompactOccludeBuffers
    {
        CompactMeshletData CompactMeshletSecondaryData;
        CompactMeshletData CompactMeshletData;
        CompactTriangleData CompactTriangleData;
        OccludeMeshletData OccludeMeshletData;
        OccludeRenderObjectData OccludeRenderObjectData;
        Buffer VisibleCountBuffer;
        Buffer OccludedCountBuffer;
        Buffer IndirectVisibleRenderObjectBuffer;
        Buffer IndirectOccludedMeshletBuffer;
        Buffer OccludeTriangleCountsBuffer;
    };
    struct CompactRenderObjectSSBO
    {
        CompactRenderObjectData CompactData;
        Buffer Buffer;
    };

    SceneCullDataUBO m_CullDataUBO{};
    SceneCullDataUBOExtended m_CullDataUBOExtended{};
    CompactRenderObjectSSBO m_CompactRenderObjectSSBO{};
    CompactOccludeBuffers m_CompactOccludeBuffers{};

    Buffer m_IndirectUncompactedBuffer{};
    Buffer m_IndirectUncompactedCountBuffer{};
    Buffer m_IndirectUncompactedOffsetBuffer{};
    
    Buffer m_IndirectDispatchBuffer{};
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

    const Buffer& GetVisibleMeshletsBuffer() const;

    const SceneCullBuffers& GetSceneCullBuffers() const { return m_SceneCullBuffers; }
private:
    void DestroyDescriptors();
    
    void PerformIndirectDispatchBufferPrepare(const FrameContext& frameContext, u32 localGroupSize, u32 bufferIndex);

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
