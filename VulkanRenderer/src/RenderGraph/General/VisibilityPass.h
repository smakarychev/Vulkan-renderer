#pragma once

#include "RenderGraph/RenderGraph.h"

#include <memory>

#include "RenderGraph/Culling/CullMetaPass.h"

class RenderPassGeometry;

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
        RenderGraph::Resource ColorOut{};
        RenderGraph::Resource DepthOut{};
        RenderGraph::Resource HiZOut{};
    };
public:
    VisibilityPass(RenderGraph::Graph& renderGraph, const VisibilityPassInitInfo& info);
    void AddToGraph(RenderGraph::Graph& renderGraph, const glm::uvec2& resolution);
    HiZPassContext* GetHiZContext() const { return m_Pass->GetHiZContext(); }
private:
    std::shared_ptr<CullMetaPass> m_Pass{};
};
