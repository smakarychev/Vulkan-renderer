#pragma once

#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"

class SceneGeometry2;

namespace Passes::SceneDepthPrepass
{
    struct ExecutionInfo
    {
        SceneDrawPassExecutionInfo DrawInfo{};
        const SceneGeometry2* Geometry{nullptr};
    };
    struct PassData
    {
        RG::DrawAttachmentResources Attachments{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
