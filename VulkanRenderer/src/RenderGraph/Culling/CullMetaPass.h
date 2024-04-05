#pragma once

#include "MeshCullPass.h"
#include "MeshletCullPass.h"
#include "TriangleCullDrawPass.h"

#include <memory>

class DrawIndirectCulledPass;
class DrawIndirectCulledContext;

struct CullMetaPassInitInfo
{
    using Features = RenderGraph::DrawFeatures;
    const RenderPassGeometry* Geometry{nullptr};
    const ShaderPipeline* DrawPipeline{nullptr};
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    Features DrawFeatures{Features::AllAttributes};
};

struct CullMetaPassExecutionInfo
{
    struct ColorInfo
    {
        RenderGraph::Resource Color{};
        AttachmentLoad OnLoad{AttachmentLoad::Load};
        RenderingAttachmentDescription::ClearValue ClearValue{};
    };
    struct DepthInfo
    {
        RenderGraph::Resource Depth{};
        AttachmentLoad OnLoad{AttachmentLoad::Load};
        RenderingAttachmentDescription::ClearValue ClearValue{};
    };
    using IBLData = RenderGraph::IBLData;
    using SSAOData = RenderGraph::SSAOData;
    
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
        std::vector<RenderGraph::Resource> ColorsOut{};
        std::optional<RenderGraph::Resource> DepthOut{};
        RenderGraph::Resource HiZOut{};
    };
public:
    CullMetaPass(RenderGraph::Graph& renderGraph, const CullMetaPassInitInfo& info, std::string_view name);
    ~CullMetaPass();
    void AddToGraph(RenderGraph::Graph& renderGraph, const CullMetaPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
    const std::string& GetName() const { return m_Name.Name(); }

    HiZPassContext* GetHiZContext() const { return m_HiZContext.get(); }
private:
    static std::vector<RenderGraph::Resource> EnsureColors(RenderGraph::Graph& renderGraph,
        const CullMetaPassExecutionInfo& info, const RenderGraph::PassName& name);
    static std::optional<RenderGraph::Resource> EnsureDepth(RenderGraph::Graph& renderGraph,
        const CullMetaPassExecutionInfo& info, const RenderGraph::PassName& name);
private:
    RenderGraph::PassName m_Name;
    PassData m_PassData;
    RenderGraph::DrawFeatures m_DrawFeatures{RenderGraph::DrawFeatures::AllAttributes};

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
};
