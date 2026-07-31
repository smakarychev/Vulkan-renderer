#pragma once

#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"

struct SceneGeometryRGResources;
class SceneLight;

namespace Passes::MomentBasedOit
{
struct ExecutionInfo
{
    SceneDrawPassExecutionInfo DrawInfo{};
    RG::ImageResource ColorIn{};
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
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}


namespace Passes::MomentBasedOitComposite
{
struct ExecutionInfo
{
    RG::ImageResource TranslucentColor{};
    RG::ImageResource OpaqueColor{};
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
    RG::ImageResource Color{};
};
PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}