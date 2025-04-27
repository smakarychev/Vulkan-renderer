#pragma once

#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"

namespace Passes::SceneDirectionalShadow
{
    struct ExecutionInfo
    {
        SceneDrawPassExecutionInfo DrawInfo{};
        const SceneGeometry* Geometry{nullptr};
    };
    struct PassData
    {
        RG::DrawAttachmentResources Attachments{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
