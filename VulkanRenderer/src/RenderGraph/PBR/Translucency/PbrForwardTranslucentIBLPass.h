#pragma once
#include <memory>

#include "RenderGraph/Culling/MeshCullPass.h"
#include "RenderGraph/General/DrawResources.h"

class MeshletCullTranslucentPass;
class MeshletCullTranslucentContext;
class DrawIndirectPass;
class MeshCullContext;

struct PbrForwardTranslucentIBLPassInitInfo
{
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    const RenderPassGeometry* Geometry{nullptr};
};

struct PbrForwardTranslucentIBLPassExecutionInfo
{
    glm::uvec2 Resolution{};
    RenderGraph::Resource ColorIn{};
    RenderGraph::Resource DepthIn{};

    RenderGraph::IBLData IBL{};
    
    const HiZPassContext* HiZContext{nullptr};
};

/* Pass that renders translucent geometry after the opaque render pass and skybox pass (if any).
 * The translucent geometry has to be sorted before being submitted to render;
 * this pass does not fully utilizes culling, because culling does not respect order (outside of subgroups),
 * therefore only per-meshlet culling w/o any compaction is performed
 */
class PbrForwardTranslucentIBLPass
{
public:
    struct PassData
    {
        RenderGraph::Resource ColorOut{};
        RenderGraph::Resource DepthOut{};
    };
public:
    PbrForwardTranslucentIBLPass(RenderGraph::Graph& renderGraph, const PbrForwardTranslucentIBLPassInitInfo& info);
    void AddToGraph(RenderGraph::Graph& renderGraph, const PbrForwardTranslucentIBLPassExecutionInfo& info);
private:
    std::shared_ptr<MeshCullContext> m_MeshContext;
    std::shared_ptr<MeshCullSinglePass> m_MeshCull;
    
    std::shared_ptr<MeshletCullTranslucentContext> m_MeshletContext;
    std::shared_ptr<MeshletCullTranslucentPass> m_MeshletCull;

    std::shared_ptr<DrawIndirectPass> m_Draw;
    
    PassData m_PassData{};
};
