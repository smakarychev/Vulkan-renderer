#pragma once

#include <glm/glm.hpp>

#include "RenderGraph/RGDrawResources.h"
#include "Core/Camera.h"

class SceneLight;
class SceneGeometry;

using DrawIndirectCountPassInitInfo = RG::DrawInitInfo;

struct DrawIndirectCountPassExecutionInfo
{
    const SceneGeometry* Geometry{nullptr};
    RG::Resource Commands{};
    /* in number of commands */
    u32 CommandsOffset{0};
    RG::Resource CommandCount{};
    /* in number of `count`, not bytes */
    u32 CountOffset{0};
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};

    RG::DrawExecutionInfo DrawInfo{};
};

namespace Passes::Draw::IndirectCount
{
    struct PassData
    {
        RG::DrawAttachmentResources DrawAttachmentResources{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const DrawIndirectCountPassExecutionInfo& info);
}
