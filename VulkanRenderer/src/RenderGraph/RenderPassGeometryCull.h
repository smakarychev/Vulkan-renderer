#pragma once

#include "RenderPassCommon.h"
#include "Rendering/Synchronization.h"

#include <memory>

class RenderingInfo;
struct FrameContext;
class ResourceUploader;
class Camera;
class DepthPyramid;
class RenderPassGeometry;

struct RenderPassGeometryCullContext
{
    CommandBuffer* Cmd{nullptr};
    u32 FrameNumber{0};
};

using DescriptorsOffsets = std::array<std::vector<u32>, MAX_PIPELINE_DESCRIPTOR_SETS>;

struct RenderPassGeometryCullRenderingContext
{
    CommandBuffer* Cmd{nullptr};
    DeletionQueue* DeletionQueue{nullptr};
    u32 FrameNumber{0};
    glm::vec2 Resolution; 
    RenderGraph::PipelineDataDescriptorSet* RenderingPipeline;
    DescriptorsOffsets DescriptorsOffsets;
    const RenderingInfo* ClearRenderingInfo;
    const RenderingInfo* CopyRenderingInfo;
    const Image* DepthBuffer;
};

class RenderPassGeometryCull
{
    struct CullContextExtended
    {
        bool Reocclusion{false};
        CommandBuffer* Cmd{nullptr};
        u32 FrameNumber{0};
        DeletionQueue* DeletionQueue{nullptr};
    };
public:
    static RenderPassGeometryCull ForGeometry(const RenderPassGeometry& renderPassGeometry,
        DescriptorAllocator& persistentAllocator, DescriptorAllocator& resolutionDependentAllocator);
    // todo: to delegate
    static void Shutdown(const RenderPassGeometryCull& renderPassGeometryCull);
    // todo: to delegate
    void SetDepthPyramid(DepthPyramid& depthPyramid, const glm::uvec2& renderResolution);
    // todo: to delegate
    void Prepare(const Camera& camera, ResourceUploader& resourceUploader, const FrameContext& frameContext);

    void CullRender(const RenderPassGeometryCullRenderingContext& context);

    const Buffer& GetTriangleBuffer() const;
    u32 GetTriangleBufferSizeBytes() const;

    static constexpr u32 TRIANGLE_OFFSET = std::numeric_limits<u32>::max(); 
private:
    void InitPipelines(DescriptorAllocator& persistentAllocator, DescriptorAllocator& resolutionDependentAllocator);
    void InitSynchronization();

    void CullMeshes(const CullContextExtended& cullContext) const;
    void CullMeshlets(const CullContextExtended& cullContext) const;
    
    void BatchIndirectDispatchesBuffersPrepare(const CullContextExtended& cullContext) const;
    void CullTriangles(const CullContextExtended& cullContext) const;
    void NextSubBatch() const;
    void ResetSubBatches() const;

    DescriptorsOffsets CreateDescriptorOffsets(const RenderPassGeometryCullRenderingContext& context) const;
    u32 GetTriangleBufferOffset() const;
    
private:
    const RenderPassGeometry* m_RenderPassGeometry{nullptr};
    
    struct CullBuffers;
    std::shared_ptr<CullBuffers> m_CullBuffers;
    class BatchedCull;
    std::shared_ptr<BatchedCull> m_BatchedCull;

    RenderGraph::PipelineDataDescriptorSet m_MeshCull{};
    RenderGraph::PipelineDataDescriptorSet m_MeshCullReocclusion{};
    RenderGraph::PipelineDataDescriptorSet m_MeshletCull{};
    RenderGraph::PipelineDataDescriptorSet m_MeshletCullReocclusion{};
    RenderGraph::PipelineDataDescriptorSet m_MeshletCullClear{};

    DependencyInfo m_ComputeWRDependency{};
    Barrier m_Barrier{};

    DepthPyramid* m_DepthPyramid{nullptr};

    DependencyInfo m_SplitBarrierDependency{};
    std::array<SplitBarrier, CULL_BATCH_OVERLAP> m_SplitBarriers{};
    bool m_ShouldCreateSplitBarriers{true};
};

