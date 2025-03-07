#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight2;
class Camera;
class SceneGeometry2;

/* this is pretty much ugb proof-of-concept */

namespace Passes::DrawSceneUnifiedBasic
{
    struct ExecutionInfo
    {
        const SceneGeometry2* Geometry{nullptr};
        const SceneLight2* Lights{nullptr};
        glm::uvec2 Resolution{};
        const Camera* Camera{nullptr};

        RG::DrawAttachments Attachments{};
    };
    struct PassData
    {
        RG::DrawAttachmentResources Attachments{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

