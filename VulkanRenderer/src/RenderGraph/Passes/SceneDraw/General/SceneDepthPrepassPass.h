#pragma once

#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"

struct SceneGeometryRGResources;

namespace Passes::SceneDepthPrepass
{
struct ExecutionInfo
{
    SceneDrawPassExecutionInfo DrawInfo{};
    const SceneGeometryRGResources* Geometry{nullptr};
    bool UseAlphaTest{false};
};

struct PassData
{
    SceneDrawPassResources Resources{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
