#pragma once

#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"

namespace Passes::SceneVBuffer
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
