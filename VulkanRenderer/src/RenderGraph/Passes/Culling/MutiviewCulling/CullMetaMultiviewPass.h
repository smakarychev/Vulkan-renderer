#pragma once

#include "MeshCullMultiviewPass.h"
#include "MeshletCullMultiviewPass.h"
#include "TriangleCullMultiviewPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/Passes/General/DrawIndirectCountPass.h"

class CullMultiviewData;

struct CullMetaMultiviewPassInitInfo
{
    CullMultiviewData* MultiviewData{nullptr};
};

class CullMetaMultiviewPass
{
public:
    struct PassData
    {
        std::vector<RG::DrawAttachmentResources> DrawAttachmentResources{};
        std::vector<RG::Resource> HiZOut{};
    };
public:
    CullMetaMultiviewPass(RG::Graph& renderGraph, std::string_view name, const CullMetaMultiviewPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph);
    Utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
    const std::string& GetName() const { return m_Name.Name(); }
private:
    void EnsureViewAttachments(RG::Graph& renderGraph, CullViewDynamicDescription& view);
    void SetAttachmentsLoadOperation(AttachmentLoad load, CullViewDynamicDescription& view);
private:
    RG::PassName m_Name;
    PassData m_PassData;

    CullMultiviewData* m_MultiviewData{nullptr};
    RG::CullMultiviewResources m_MultiviewResource{};
    RG::CullTrianglesMultiviewResource m_MultiviewTrianglesResource{};
    
    using MeshCull = MeshCullMultiviewPass;
    using MeshletCull = MeshletCullMultiviewPass;
    using TrianglePrepare = TriangleCullPrepareMultiviewPass;
    using TriangleCull = TriangleCullMultiviewPass;

    std::vector<u32> m_MeshletOnlyViewIndices;

    std::vector<std::unique_ptr<HiZPass>> m_HiZs;
    std::vector<std::unique_ptr<HiZPass>> m_HiZsReocclusion;

    std::unique_ptr<MeshCull> m_MeshCull;
    std::unique_ptr<MeshCull> m_MeshReocclusion;
    
    std::unique_ptr<MeshletCull> m_MeshletCull;
    std::unique_ptr<MeshletCull> m_MeshletReocclusion;

    std::unique_ptr<TrianglePrepare> m_TrianglePrepare;
    std::unique_ptr<TrianglePrepare> m_TrianglePrepareReocclusion;

    std::unique_ptr<TriangleCull> m_TriangleCullDraw;
    std::unique_ptr<TriangleCull> m_TriangleReocclusionDraw;
    
    std::vector<std::unique_ptr<DrawIndirectCountPass>> m_MeshletOnlyDraws;
    std::vector<std::unique_ptr<DrawIndirectCountPass>> m_DrawsReocclusion;
};
