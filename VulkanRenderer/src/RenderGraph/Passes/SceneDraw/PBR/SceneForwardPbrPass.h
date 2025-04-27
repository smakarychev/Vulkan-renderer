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
        const SceneLight* Lights{nullptr};
        RG::SSAOData SSAO{};
        RG::IBLData IBL{};
        RG::Resource Clusters{};
        RG::Resource Tiles{};
        RG::Resource ZBins{};
    };
    struct PassData
    {
        RG::DrawAttachmentResources Attachments{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

