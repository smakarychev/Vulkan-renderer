#pragma once

#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"

class SceneLight;
class SceneGeometry;

namespace Passes::SceneForwardPbr
{
    struct ExecutionInfo
    {
        SceneDrawPassExecutionInfo DrawInfo{};
        std::optional<ShaderOverrides> CommonOverrides{std::nullopt};
        const SceneGeometry* Geometry{nullptr};
        const SceneLight* Light{nullptr};
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

