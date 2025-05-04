#pragma once

#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"

class SceneGeometry;

namespace Passes::SceneDepthPrepass
{
    struct ExecutionInfo
    {
        SceneDrawPassExecutionInfo DrawInfo{};
        const SceneGeometry* Geometry{nullptr};
    };
    struct PassData
    {
        SceneDrawPassResources Resources{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
