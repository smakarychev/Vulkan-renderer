#pragma once
#include <glm/fwd.hpp>

#include "ModelAsset.h"
#include "Settings.h"
#include "Core/Camera.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Shader.h"
#include "Vulkan/VulkanUtils.h"

class SceneCull;
struct FrameContext;
class ResourceUploader;
class DepthPyramid;
class Scene;

class CullDrawBatch
{
public:
    static constexpr u32 MAX_TRIANGLES = 128'000;
    static constexpr u32 MAX_INDICES = MAX_TRIANGLES * 3;
    static constexpr u32 MAX_COMMANDS = MAX_TRIANGLES / assetLib::ModelInfo::TRIANGLES_PER_MESHLET;
public:
    CullDrawBatch();
    ~CullDrawBatch();
    CullDrawBatch(const CullDrawBatch&) = delete;
    CullDrawBatch(const CullDrawBatch&&) = delete;
    CullDrawBatch& operator=(const CullDrawBatch&) = delete;
    CullDrawBatch& operator=(CullDrawBatch&&) = delete;

    const Buffer& GetCountBuffer() const { return m_Count; }
    const Buffer& GetIndices() const { return m_Indices; }
    u32 GetCommandCount() const { return MAX_COMMANDS * m_SubBatchCount; }

    static u64 GetCommandsSizeBytes()
    {
        return vkUtils::alignUniformBufferSizeBytes(MAX_COMMANDS * sizeof(IndirectCommand) * SUB_BATCH_COUNT);
    }
    static u64 GetTrianglesSizeBytes()
    {
        return vkUtils::alignUniformBufferSizeBytes(MAX_TRIANGLES * sizeof(u32) * SUB_BATCH_COUNT);
    }
    
private:
    u32 m_SubBatchCount{SUB_BATCH_COUNT};
    Buffer m_Count;
    Buffer m_Indices;
};

class SceneCullBuffers
{
public:
    static constexpr u32 MAX_COMMAND_COUNT = MAX_DRAW_INDIRECT_CALLS + CullDrawBatch::MAX_COMMANDS * SUB_BATCH_COUNT;
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
    void Shutdown();
    void Update(const Camera& camera, const DepthPyramid* depthPyramid,  ResourceUploader& resourceUploader, const FrameContext& frameContext);

    const Buffer& GetCullData() const { return m_CullDataUBO.Buffer; }
    const Buffer& GetCullDataExtended() const { return m_CullDataUBOExtended.Buffer; }
    
    const Buffer& GetVisibleMeshletCount() const { return m_CountBuffers.VisibleMeshlets; }
    
    const Buffer& GetMeshVisibility() const { return m_VisibilityBuffers.MeshVisibility; }
    const Buffer& GetMeshletVisibility() const { return m_VisibilityBuffers.MeshletVisibility; }
    const Buffer& GetTriangleVisibility() const { return m_VisibilityBuffers.TriangleVisibility; }
    
    const Buffer& GetBatchIndirectDispatches() const { return m_BatchIndirectDispatches; }
    const Buffer& GetBatchCompactIndirectDispatches() const { return m_BatchClearIndirectDispatches; }

    u32 GetVisibleMeshletsCountValue(u32 frameNumber) const;

    const Buffer& GetCompactedCommands() const { return m_CompactedCommands; }
    const Buffer& GetCompactedBatchCommands() const { return m_CompactedBatchCommands; }
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
        Buffer VisibleMeshlets;
    };
    struct VisibilityBuffers
    {
        Buffer MeshVisibility;
        Buffer MeshletVisibility;
        Buffer TriangleVisibility;
    };

    SceneCullDataUBO m_CullDataUBO{};
    SceneCullDataUBOExtended m_CullDataUBOExtended{};
    CountBuffers m_CountBuffers{};
    VisibilityBuffers m_VisibilityBuffers;

    void* m_VisibleMeshletCountBufferMappedAddress{nullptr};

    Buffer m_CompactedCommands{};
    Buffer m_CompactedBatchCommands{};
    
    Buffer m_BatchIndirectDispatches{};
    Buffer m_BatchClearIndirectDispatches{};
};

struct CullSettings
{
    bool Reocclusion{false};
    bool TrianglePassOnly{false};
};

class SceneBatchCull
{
    struct ComputePipelineData;
public:
    SceneBatchCull(Scene& scene, SceneCullBuffers& sceneCullBuffers);
    void Init(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void Shutdown();

    void SetDepthPyramid(const DepthPyramid& depthPyramid, const Buffer& triangles, u64 trianglesSizeBytes, u64 trianglesOffset);
    
    void BatchIndirectDispatchesBuffersPrepare(const FrameContext& frameContext);
    void CullTriangles(const FrameContext& frameContext, const CullSettings& cullSettings);
    const CullDrawBatch& GetCullDrawBatch() const { return *m_CullDrawBatches[m_CurrentBatch]; }
    void NextSubBatch();
    void ResetSubBatches();

    u64 GetTrianglesSizeBytes() const { return  CullDrawBatch::GetTrianglesSizeBytes(); }
    u64 GetTrianglesOffset() const { return CullDrawBatch::GetTrianglesSizeBytes() * m_CurrentBatch; }

    u64 GetDrawCommandsSizeBytes() const { return CullDrawBatch::GetCommandsSizeBytes(); }
    u64 GetDrawCommandsOffset() const { return CullDrawBatch::GetCommandsSizeBytes() * m_CurrentBatch; }
    u32 GetMaxDrawCommandCount() const { return m_CullDrawBatches.front()->GetCommandCount(); }
    
    const Buffer& GetDrawCount() const;
    u32 GetMaxBatchCount() const { return m_MaxBatchDispatches; }
    u32 ReadBackBatchCount(const FrameContext& frameContext) const;
private:
    void DestroyDescriptors();
    
    void InitBatchCull(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
private:
    Scene* m_Scene{nullptr};
    SceneCull* m_SceneCull{nullptr};
    SceneCullBuffers* m_SceneCullBuffers{nullptr};
    bool m_CullIsInitialized{false};
    
    std::array<std::unique_ptr<CullDrawBatch>, CULL_DRAW_BATCH_OVERLAP> m_CullDrawBatches;

    struct ComputePipelineData
    {
        ShaderPipelineTemplate* Template;
        ShaderPipeline Pipeline;
        ShaderDescriptorSet DescriptorSet;
    };
    
    struct ComputeBatchData
    {
        ComputePipelineData ClearCount{};
        ComputePipelineData TriangleCull{};
        ComputePipelineData TriangleCullReocclusion{};
        ComputePipelineData CompactCommands{};
        ComputePipelineData ClearCommands{};
    };
    std::array<ComputeBatchData, CULL_DRAW_BATCH_OVERLAP> m_CullDrawBatchData{};
    ComputePipelineData m_PrepareIndirectDispatches{};
    ComputePipelineData m_PrepareCompactIndirectDispatches{};
    
    u32 m_CurrentBatch{0};
    u32 m_CurrentBatchFlat{0};
    u32 m_MaxBatchDispatches{0};

    PipelineBufferBarrierInfo m_ComputeWRBarrierBase{};
    PipelineBufferBarrierInfo m_IndirectWRBarrierBase{};
};

class SceneCull
{
public:
    void Init(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void Shutdown();

    void SetDepthPyramid(const DepthPyramid& depthPyramid);
    void UpdateBuffers(const Camera& camera, ResourceUploader& resourceUploader, const FrameContext& frameContext);

    const SceneCullBuffers& GetSceneCullBuffers() const;

    void CullMeshes(const FrameContext& frameContext, bool reocclusion);
    void CullMeshlets(const FrameContext& frameContext, bool reocclusion);
    
    void BatchIndirectDispatchesBuffersPrepare(const FrameContext& frameContext);
    void CullCompactTrianglesBatch(const FrameContext& frameContext, const CullSettings& cullSettings);
    const CullDrawBatch& GetCullDrawBatch() const;
    void NextSubBatch();
    void ResetSubBatches();

    const Buffer& GetDrawCommands() const;
    const Buffer& GetTriangles() const;
    u64 GetTrianglesSizeBytes() const;

    u64 GetDrawCommandsSizeBytes() const;
    u32 GetMaxDrawCommandCount() const;

    u64 GetDrawCommandsOffset() const;
    u64 GetDrawTrianglesOffset() const;

    const Buffer& GetDrawCount() const;
    u32 GetMaxBatchCount() const;
    u32 ReadBackBatchCount(const FrameContext& frameContext) const;

    SceneBatchCull& GetBatchCull();
    const SceneBatchCull& GetBatchCull() const;

private:
    void DestroyDescriptors();
    
    void InitBarriers();
    
    void InitMeshCull(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);
    void InitMeshletCull(Scene& scene, DescriptorAllocator& allocator, DescriptorLayoutCache& layoutCache);

private:
    Scene* m_Scene{nullptr};
    SceneCullBuffers m_SceneCullBuffers{};
    std::unique_ptr<SceneBatchCull> m_SceneBatchCull;

    Buffer m_Triangles{};
    u64 m_TrianglesOffsetBase{0};
    
    struct ComputePipelineData
    {
        ShaderPipelineTemplate* Template;
        ShaderPipeline Pipeline;
        ShaderDescriptorSet DescriptorSet;
    };
    
    ComputePipelineData m_MeshCull{};
    ComputePipelineData m_MeshCullReocclusion{};
    ComputePipelineData m_MeshletCull{};
    ComputePipelineData m_MeshletCullReocclusion{};
    ComputePipelineData m_MeshletCullClear{};
    
    PipelineBufferBarrierInfo m_ComputeWRBarrierBase{};
    PipelineBufferBarrierInfo m_ComputeRWBarrierBase{};
    PipelineBufferBarrierInfo m_IndirectWRBarrierBase{};
    
    const DepthPyramid* m_DepthPyramid{nullptr};
    bool m_CullIsInitialized{false};
};
