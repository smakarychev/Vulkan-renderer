#pragma once

#include "RenderGraph/Passes/Culling/CullMetaPass.h"

struct DirectionalShadowPassInitInfo
{
    const RG::Geometry* Geometry{nullptr};
};

struct DirectionalShadowPassExecutionInfo
{
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
};

class DirectionalShadowPass
{
public:
    struct PassData
    {
        RG::Resource ShadowMap{};
    };
public:
    DirectionalShadowPass(RG::Graph& renderGraph, const DirectionalShadowPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const DirectionalShadowPassExecutionInfo& info);
private:
    std::shared_ptr<CullMetaPass> m_Pass{};
};
