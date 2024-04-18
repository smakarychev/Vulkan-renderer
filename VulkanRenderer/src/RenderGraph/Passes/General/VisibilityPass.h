#pragma once

#include "RenderGraph/RenderGraph.h"

#include <memory>

#include "RenderGraph/Passes/Culling/CullMetaPass.h"

struct VisibilityPassInitInfo
{
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    const RG::Geometry* Geometry{nullptr};
};

class VisibilityPass
{
public:
    struct PassData
    {
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};
        RG::Resource HiZOut{};
    };
public:
    VisibilityPass(RG::Graph& renderGraph, const VisibilityPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const glm::uvec2& resolution, const Camera* camera);
    HiZPassContext* GetHiZContext() const { return m_Pass->GetHiZContext(); }
private:
    std::shared_ptr<CullMetaPass> m_Pass{};
};
