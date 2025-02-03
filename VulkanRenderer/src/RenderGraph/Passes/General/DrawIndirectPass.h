#pragma once
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RGCommon.h"

class Shader;
class SceneLight;
class Camera;

class SceneGeometry;

struct DrawIndirectPassExecutionInfo
{
    const SceneGeometry* Geometry{nullptr};
    RG::Resource Commands{};
    u32 CommandsOffset{0};
    glm::uvec2 Resolution{};
    const Camera* Camera{nullptr};

    RG::DrawExecutionInfo DrawInfo{};
};

namespace Passes::Draw::Indirect
{
    struct PassData
    {
        RG::DrawAttachmentResources DrawAttachmentResources{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const DrawIndirectPassExecutionInfo& info);
}