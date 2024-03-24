#pragma once

#include "MeshCullPass.h"
#include "MeshletCullPass.h"
#include "TriangleCullDrawPass.h"

#include <memory>

class DrawIndirectCulledPass;
class DrawIndirectCulledContext;

struct CullMetaPassInitInfo
{
    ShaderPipelineTemplate* DrawTemplate{nullptr};
    glm::uvec2 Resolution;
    RenderPassGeometry* Geometry{nullptr};
};

struct CullMetaPassExecutionInfo
{
    FrameContext* FrameContext{nullptr};
    std::vector<RenderGraph::Resource> Colors{};
    std::optional<RenderGraph::Resource> Depth{};
};

class CullMetaPass
{
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
private:
    std::vector<RenderGraph::Resource> EnsureColors(RenderGraph::Graph& renderGraph,
        const CullMetaPassExecutionInfo& info) const;
    std::optional<RenderGraph::Resource> EnsureDepth(RenderGraph::Graph& renderGraph,
        const CullMetaPassExecutionInfo& info) const;
private:
    RenderGraph::PassName m_Name;
    PassData m_PassData;

    using HiZ = HiZPass;
    
    using MeshCull = MeshCullPass;
    using MeshReocclusion = MeshCullReocclusionPass;

    using MeshletCull = MeshletCullPass;
    using MeshletReocclusion = MeshletCullReocclusionPass;

    using TrianglePrepareDispatch = TriangleCullPrepareDispatchPass<false>;
    using TrianglePrepareReocclusionDispatch = TriangleCullPrepareDispatchPass<true>;

    using Draw = DrawIndirectCulledPass;

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

    using TriangleCullDraw = TriangleCullDrawPass<false>;
    using TriangleReoccludeDraw = TriangleCullDrawPass<true>;
    
    std::shared_ptr<TriangleCullDraw> m_CullDraw;
    std::shared_ptr<TriangleReoccludeDraw> m_ReoccludeTrianglesDraw;
    std::shared_ptr<TriangleReoccludeDraw> m_ReoccludeDraw;
};
