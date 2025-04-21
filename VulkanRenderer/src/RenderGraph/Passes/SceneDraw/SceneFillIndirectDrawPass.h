#pragma once

#include "RenderGraph/RenderPass.h"
#include "Scene/ScenePass.h"

class SceneGeometry2;
class SceneRenderObjectSet;

namespace Passes::SceneFillIndirectDraw
{
    struct ExecutionInfo
    {
        const SceneGeometry2* Geometry{nullptr};
        std::array<RG::Resource, MAX_BUCKETS_PER_SET> Draws;
        std::array<RG::Resource, MAX_BUCKETS_PER_SET> DrawInfos;
        u32 BucketCount{0};
        RG::Resource MeshletInfos{};
        RG::Resource MeshletInfoCount{};
    };
    struct PassData
    {
        std::array<RG::Resource, MAX_BUCKETS_PER_SET> Draws;
        std::array<RG::Resource, MAX_BUCKETS_PER_SET> DrawInfos;
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
