#pragma once

#include "RenderGraph/Passes/SceneDraw/SceneDrawPassesCommon.h"

class Camera;
class SceneLight2;
class SceneGeometry2;

namespace Passes::SceneUnifiedPbr
{
    struct ExecutionInfo
    {
        SceneDrawPassExecutionInfo DrawInfo{};
        const SceneGeometry2* Geometry{nullptr};
        const SceneLight2* Lights{nullptr};
    };
    struct PassData
    {
        RG::DrawAttachmentResources Attachments{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

