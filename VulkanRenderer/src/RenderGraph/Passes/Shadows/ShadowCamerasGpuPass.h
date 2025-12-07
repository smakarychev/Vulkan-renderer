#pragma once
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGResource.h"

// todo: finish me! so far the greatest problem is that I did not account for a fact that camera might come from gpu...
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
        RG::Resource ViewInfo{};
        RG::Resource DepthMinMax{};
        RG::Resource CsmData{};
        std::array<RG::Resource, SHADOW_CASCADES> ShadowViews{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
