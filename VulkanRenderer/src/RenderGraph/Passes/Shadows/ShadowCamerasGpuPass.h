#pragma once

#include "Settings.h"
#include "RenderGraph/RGResource.h"

namespace Passes::ShadowCamerasGpu
{
struct ExecutionInfo
{
    RG::Resource DepthMinMax{};
    RG::Resource View{};
    glm::vec3 LightDirection{};
};

struct PassData
{
    RG::Resource CsmData{};
    std::array<RG::Resource, SHADOW_CASCADES> ShadowViews{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
