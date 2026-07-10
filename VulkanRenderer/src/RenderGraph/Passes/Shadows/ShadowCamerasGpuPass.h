#pragma once

#include "Settings.h"
#include "RenderGraph/RGResource.h"

namespace Passes::ShadowCamerasGpu
{
struct ExecutionInfo
{
    RG::BufferResource DepthMinMax{};
    RG::BufferResource View{};
    glm::vec3 LightDirection{};
};

struct PassData
{
    RG::BufferResource CsmData{};
    std::array<RG::BufferResource, SHADOW_CASCADES> ShadowViews{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
