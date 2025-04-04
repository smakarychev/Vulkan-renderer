#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class Camera;
class SceneLight2;
class SceneGeometry2;

/* this is pretty much ugb proof-of-concept */

namespace Passes::DrawSceneUnifiedBasic
{
    struct ExecutionInfo
    {
        const SceneGeometry2* Geometry{nullptr};
        const SceneLight2* Lights{nullptr};
        Buffer Draws{};
        Buffer DrawInfos{};
        glm::uvec2 Resolution{};
        const Camera* Camera{nullptr};

        RG::DrawAttachments Attachments{};
    };
    struct PassData
    {
        RG::DrawAttachmentResources Attachments{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

