#pragma once

#include "RenderGraph/RenderGraph.h"

#include <memory>

class RenderPassGeometry;
class CullMetaPass;

struct VisibilityPassInitInfo
{
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    const RenderPassGeometry* Geometry{nullptr};
};

class VisibilityPass
{
public:
    struct PassData
    {
        RenderGraph::Resource ColorsOut{};
        RenderGraph::Resource DepthOut{};
        RenderGraph::Resource HiZOut{};
    };
public:
    VisibilityPass(RenderGraph::Graph& renderGraph, const VisibilityPassInitInfo& info);
    void AddToGraph(RenderGraph::Graph& renderGraph, const glm::uvec2& resolution);
private:
    std::shared_ptr<CullMetaPass> m_Pass{};
    
};
