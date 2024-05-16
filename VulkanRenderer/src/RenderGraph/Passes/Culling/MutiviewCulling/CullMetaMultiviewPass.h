#pragma once

#include "MeshCullMultiviewPass.h"
#include "MeshletCullMultiviewPass.h"
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
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
    const std::string& GetName() const { return m_Name.Name(); }
private:
    void EnsureViewAttachments(RG::Graph& renderGraph, CullViewDynamicDescription& view);
    void SetAttachmentsLoadOperation(AttachmentLoad load, CullViewDynamicDescription& view);
    void RecordUpdatedAttachmentResources(const RG::DrawAttachments& old, const RG::DrawAttachmentResources& updated);
    void UpdateRecordedAttachmentResources(RG::DrawAttachments& attachments);
private:
    RG::PassName m_Name;
    PassData m_PassData;

    std::unordered_map<RG::Resource, RG::Resource> m_AttachmentRenames;

    CullMultiviewData* m_MultiviewData{nullptr};
    RG::CullMultiviewResources m_MultiviewResource{};
    
    using MeshCull = MeshCullMultiviewPass;
    using MeshletCull = MeshletCullMultiviewPass;

    std::vector<std::unique_ptr<HiZPass>> m_HiZs;
    std::vector<std::unique_ptr<HiZPass>> m_HiZsReocclusion;

    std::unique_ptr<MeshCull> m_MeshCull;
    std::unique_ptr<MeshCull> m_MeshReocclusion;
    
    std::unique_ptr<MeshletCull> m_MeshletCull;
    std::unique_ptr<MeshletCull> m_MeshletReocclusion;

    // todo: this is temp, change once triangle culling is done
    std::vector<std::unique_ptr<DrawIndirectCountPass>> m_Draws;
    std::vector<std::unique_ptr<DrawIndirectCountPass>> m_DrawsReocclusion;
};
