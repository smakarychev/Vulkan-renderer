#pragma once

#include "TriangleCullDrawPass.h"
#include "RenderGraph/RenderGraph.h"

struct CullMetaPassExecutionInfo;
struct CullMetaPassInitInfo;

class CullMetaSinglePass
{
public:
    struct PassData
    {
        std::vector<RenderGraph::Resource> ColorsOut{};
        std::optional<RenderGraph::Resource> DepthOut{};
    };
public:
    CullMetaSinglePass(RenderGraph::Graph& renderGraph, const CullMetaPassInitInfo& info, std::string_view name);
    void AddToGraph(RenderGraph::Graph& renderGraph, const CullMetaPassExecutionInfo& info,
        HiZPassContext& hiZContext);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
    const std::string& GetName() const { return m_Name.Name(); }
private:
    RenderGraph::PassName m_Name;
    PassData m_PassData;

    RenderGraph::DrawFeatures m_DrawFeatures{RenderGraph::DrawFeatures::AllAttributes};

    using MeshCull = MeshCullSinglePass;
    using MeshletCull = MeshletCullSinglePass;
    using TrianglePrepareDispatch = TriangleCullPrepareDispatchPass<CullStage::Single>;

    std::shared_ptr<MeshCullContext> m_MeshContext;
    std::shared_ptr<MeshCull> m_MeshCull;
    
    std::shared_ptr<MeshletCullContext> m_MeshletContext;
    std::shared_ptr<MeshletCull> m_MeshletCull;

    std::shared_ptr<TriangleCullContext> m_TriangleContext;
    std::shared_ptr<TriangleDrawContext> m_TriangleDrawContext;
    std::shared_ptr<TrianglePrepareDispatch> m_TrianglePrepareDispatch; 

    using TriangleCullDraw = TriangleCullDrawPass<CullStage::Single>;
    
    std::shared_ptr<TriangleCullDraw> m_CullDraw;
};
