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

    struct CompactMeshletData
    {
        u32 DrawCount;
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
    const Buffer& GetVisibleRenderObjectCountBuffer() const { return m_CompactRenderObjectSSBO.Buffer; }
    const Buffer& GetVisibleCountBuffer() const { return m_CompactOccludeBuffers.VisibleCountBuffer; }
    const Buffer& GetVisibleCountSecondaryBuffer() const { return m_CompactOccludeBuffers.VisibleCountSecondaryBuffer; }
    const Buffer& GetOccludedCountBuffer() const { return m_CompactOccludeBuffers.OccludedCountBuffer; }
    const Buffer& GetIndirectVisibleRenderObjectBuffer() const { return m_CompactOccludeBuffers.IndirectVisibleRenderObjectBuffer; }
    const Buffer& GetIndirectOccludedMeshletBuffer() const { return m_CompactOccludeBuffers.IndirectOccludedMeshletBuffer; }
    const Buffer& GetIndirectDispatchBuffer() const { return m_IndirectDispatchBuffer; }

private:
    struct SceneCullDataUBO
    {
        SceneCullData SceneData;
        Buffer Buffer;
    };
    struct CompactOccludeBuffers
    {
        CompactMeshletData CompactMeshletSecondaryData;
        CompactMeshletData CompactMeshletData;
        OccludeMeshletData OccludeMeshletData;
        OccludeRenderObjectData OccludeRenderObjectData;
        Buffer VisibleCountBuffer;
        Buffer VisibleCountSecondaryBuffer;
        Buffer OccludedCountBuffer;
        Buffer IndirectVisibleRenderObjectBuffer;
        Buffer IndirectOccludedMeshletBuffer;
    };
    struct CompactRenderObjectSSBO
    {
        CompactRenderObjectData CompactData;
        Buffer Buffer;
    };

    SceneCullDataUBO m_CullDataUBO{};
    CompactRenderObjectSSBO m_CompactRenderObjectSSBO{};
    CompactOccludeBuffers m_CompactOccludeBuffers{};

    Buffer m_IndirectDispatchBuffer{};
};

class SceneCull
{
    enum class CullStage {Primary, Secondary};
public:
    void Init(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void ShutDown();

    void SetDepthPyramid(const DepthPyramid& depthPyramid);
    void UpdateBuffers(const Camera& camera, ResourceUploader& resourceUploader, const FrameContext& frameContext);
    void ResetCullBuffers(const FrameContext& frameContext);
    
    void PerformMeshCulling(const FrameContext& frameContext);
    void PerformMeshletCulling(const FrameContext& frameContext);
    void PerformMeshCompaction(const FrameContext& frameContext);
    void PerformMeshletCompaction(const FrameContext& frameContext);

    void PerformSecondaryMeshCulling(const FrameContext& frameContext);
    void PerformSecondaryMeshletCulling(const FrameContext& frameContext);
    void PerformSecondaryMeshCompaction(const FrameContext& frameContext);
    void PerformSecondaryMeshletCompaction(const FrameContext& frameContext);

    const Buffer& GetVisibleMeshletsBuffer() const;
private:
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
private:
    CullStage m_CullStage{CullStage::Primary};
    Scene* m_Scene{nullptr};
    SceneCullBuffers m_SceneCullBuffers{};

    struct ComputePipelineData
    {
        ShaderPipelineTemplate* Template;
        ShaderPipeline Pipeline;
        ShaderDescriptorSet DescriptorSet;
    };
    
    ComputePipelineData m_ClearBuffersData{};
    
    ComputePipelineData m_MeshCullData{};
    ComputePipelineData m_MeshletCullData{};
    ComputePipelineData m_MeshletCompactData{};
    ComputePipelineData m_MeshCompactVisibleData{};

    ComputePipelineData m_PrepareIndirectDispatch{};

    ComputePipelineData m_MeshCullSecondaryData{};
    ComputePipelineData m_MeshletCullSecondaryData{};
    ComputePipelineData m_MeshCompactVisibleSecondaryData{};
    ComputePipelineData m_MeshletCompactSecondaryData{};
    
    
    const DepthPyramid* m_DepthPyramid{nullptr};
    bool m_CullIsInitialized{false};
};
