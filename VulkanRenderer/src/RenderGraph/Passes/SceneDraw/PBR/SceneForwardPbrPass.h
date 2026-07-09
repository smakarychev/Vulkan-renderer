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
    RG::Resource DirectionalLights{};
    RG::Resource PointLights{};
    RG::SSAOData SSAO{};
    RG::IBLData IBL{};
    RG::Resource Clusters{};
    RG::Resource Tiles{};
    RG::Resource ZBins{};
    RG::CsmData CsmData{};
};

struct PassData
{
    SceneDrawPassResources Resources{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
