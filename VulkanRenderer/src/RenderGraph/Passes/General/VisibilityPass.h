#pragma once

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Culling/MutiviewCulling/CullMultiviewData.h"

struct VisibilityPassExecutionInfo
{
    const SceneGeometry* Geometry{nullptr};
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};
};

namespace Passes::Draw::Visibility
{
    struct PassData
    {
        RG::Resource ColorOut{};
        RG::Resource DepthOut{};
        RG::Resource HiZOut{};
        RG::Resource HiZMaxOut{};
        RG::Resource MinMaxDepth{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const VisibilityPassExecutionInfo& info);
}
