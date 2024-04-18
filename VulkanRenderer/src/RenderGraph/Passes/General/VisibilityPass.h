#pragma once

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Culling/CullMetaPass.h"

#include <memory>

struct VisibilityPassInitInfo
{
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    const RG::Geometry* Geometry{nullptr};
};

struct VisibilityPassExecutionInfo
{
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
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
    void AddToGraph(RG::Graph& renderGraph, const VisibilityPassExecutionInfo& info);
    HiZPassContext* GetHiZContext() const { return m_Pass->GetHiZContext(); }
private:
    std::shared_ptr<CullMetaPass> m_Pass{};
};
