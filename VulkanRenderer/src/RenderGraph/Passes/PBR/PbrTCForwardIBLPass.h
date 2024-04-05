#pragma once

#include "RenderGraph/RenderGraph.h"

#include <memory>

#include "RenderGraph/RGDrawResources.h"

class CullMetaPass;
class HiZPassContext;
class ShaderDescriptors;
class RenderPassGeometry;

struct PbrForwardIBLPassInitInfo
{
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    const RenderPassGeometry* Geometry{nullptr};
};

struct PbrForwardIBLPassExecutionInfo
{
    glm::uvec2 Resolution{};
    RenderGraph::Resource ColorIn{};
    RenderGraph::Resource DepthIn{};

    RenderGraph::IBLData IBL{};
};

/* Main forward pbr ibl pass
 * 'TC' stands for triangle culling
 * This pass is suitable to draw opaque surfaces only
 */
class PbrTCForwardIBLPass
{
public:
    struct PassData
    {
        RenderGraph::Resource ColorOut{};
        RenderGraph::Resource DepthOut{};
    };
public:
    PbrTCForwardIBLPass(RenderGraph::Graph& renderGraph, const PbrForwardIBLPassInitInfo& info, std::string_view name);
    void AddToGraph(RenderGraph::Graph& renderGraph, const PbrForwardIBLPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RenderGraph::PassName m_Name;
    std::shared_ptr<CullMetaPass> m_Pass{};
};
