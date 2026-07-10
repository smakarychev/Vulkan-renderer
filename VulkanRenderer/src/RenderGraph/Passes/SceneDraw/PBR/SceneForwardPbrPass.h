#pragma once

#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"

struct SceneGeometryRGResources;
class SceneLight;

namespace Passes::SceneForwardPbr
{
struct ExecutionInfo
{
    SceneDrawPassExecutionInfo DrawInfo{};
    std::optional<ShaderOverrides> CommonOverrides{std::nullopt};
    const SceneGeometryRGResources* Geometry{nullptr};
    RG::BufferResource DirectionalLights{};
    RG::BufferResource PointLights{};
    RG::SSAOData SSAO{};
    RG::IBLData IBL{};
    RG::BufferResource Clusters{};
    RG::BufferResource Tiles{};
    RG::BufferResource ZBins{};
    RG::CsmData CsmData{};
};

struct PassData
{
    SceneDrawPassResources Resources{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
