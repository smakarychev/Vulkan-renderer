#pragma once

#include "SceneDrawPassesCommon.h"
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class Camera;
class SceneLight2;
class SceneGeometry2;

/* this is pretty much ugb proof-of-concept */

namespace Passes::SceneDrawUnifiedBasic
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

