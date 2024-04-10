#pragma once

#include "MeshCullPass.h"
#include "MeshletCullPass.h"
#include "TriangleCullDrawPass.h"

#include <memory>

class DrawIndirectCountPass;

struct CullMetaPassInitInfo
{
    using Features = RG::DrawFeatures;
    const RG::Geometry* Geometry{nullptr};
    const ShaderPipeline* DrawTrianglesPipeline{nullptr};
    const ShaderPipeline* DrawMeshletsPipeline{nullptr};
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    Features DrawFeatures{Features::AllAttributes};
};

struct CullMetaPassExecutionInfo
{
    struct ColorInfo
    {
        RG::Resource Color{};
        AttachmentLoad OnLoad{AttachmentLoad::Load};
        RenderingAttachmentDescription::ClearValue ClearValue{};
    };
    struct DepthInfo
    {
        RG::Resource Depth{};
        AttachmentLoad OnLoad{AttachmentLoad::Load};
        RenderingAttachmentDescription::ClearValue ClearValue{};
    };
    using IBLData = RG::IBLData;
    using SSAOData = RG::SSAOData;
    
    glm::uvec2 Resolution;
    std::vector<ColorInfo> Colors{};
    std::optional<DepthInfo> Depth{};   
    std::optional<IBLData> IBL{};
    std::optional<SSAOData> SSAO{};
};

class CullMetaPass
{
    friend class CullMetaSinglePass;
public:
    struct PassData
    {
        RG::DrawAttachmentResources DrawAttachmentResources{};

        RG::Resource HiZOut{};
    };
public:
    CullMetaPass(RG::Graph& renderGraph, const CullMetaPassInitInfo& info, std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, const CullMetaPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
    const std::string& GetName() const { return m_Name.Name(); }

    HiZPassContext* GetHiZContext() const { return m_HiZContext.get(); }
private:
    static std::vector<RG::Resource> EnsureColors(RG::Graph& renderGraph,
        const CullMetaPassExecutionInfo& info, const RG::PassName& name);
    static std::optional<RG::Resource> EnsureDepth(RG::Graph& renderGraph,
        const CullMetaPassExecutionInfo& info, const RG::PassName& name);
private:
    RG::PassName m_Name;
    PassData m_PassData;
    RG::DrawFeatures m_DrawFeatures{RG::DrawFeatures::AllAttributes};

    using HiZ = HiZPass;
    
    using MeshCull = MeshCullPass;
    using MeshReocclusion = MeshCullReocclusionPass;

    using MeshletCull = MeshletCullPass;
    using MeshletReocclusion = MeshletCullReocclusionPass;

    using TrianglePrepareDispatch = TriangleCullPrepareDispatchPass<CullStage::Cull>;
    using TrianglePrepareReocclusionDispatch = TriangleCullPrepareDispatchPass<CullStage::Reocclusion>;

    std::shared_ptr<HiZPassContext> m_HiZContext;
    std::shared_ptr<HiZ> m_HiZ;
    std::shared_ptr<HiZ> m_HiZReocclusion;
    
    std::shared_ptr<MeshCullContext> m_MeshContext;
    std::shared_ptr<MeshCull> m_MeshCull;
    std::shared_ptr<MeshReocclusion> m_MeshReocclusion;
    
    std::shared_ptr<MeshletCullContext> m_MeshletContext;
    std::shared_ptr<MeshletCull> m_MeshletCull;
    std::shared_ptr<MeshletReocclusion> m_MeshletReocclusion;

    std::shared_ptr<TriangleCullContext> m_TriangleContext;
    std::shared_ptr<TriangleDrawContext> m_TriangleDrawContext;
    std::shared_ptr<TrianglePrepareDispatch> m_TrianglePrepareDispatch; 
    std::shared_ptr<TrianglePrepareReocclusionDispatch> m_TrianglePrepareReocclusionDispatch;

    using TriangleCullDraw = TriangleCullDrawPass<CullStage::Cull>;
    using TriangleReoccludeDraw = TriangleCullDrawPass<CullStage::Reocclusion>;
    
    std::shared_ptr<TriangleCullDraw> m_CullDraw;
    std::shared_ptr<TriangleReoccludeDraw> m_ReoccludeTrianglesDraw;
    std::shared_ptr<TriangleReoccludeDraw> m_ReoccludeDraw;

    std::shared_ptr<DrawIndirectCountPass> m_DrawIndirectCountPass;
};
