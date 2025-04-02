#pragma once
#include "RenderGraph/RenderPass.h"
#include "Scene/ScenePass.h"

class SceneGeometry2;
class SceneRenderObjectSet;

namespace Passes::FillSceneIndirectDraw
{
    struct ExecutionInfo
    {
        const SceneGeometry2* Geometry{nullptr};
        const SceneRenderObjectSet* RenderObjectSet{nullptr};
        RG::Resource MeshletInfos{};
        RG::Resource MeshletInfoCount{};

        // todo: this should consider visibility
    };
    struct PassData
    {
        std::array<RG::Resource, MAX_BUCKETS_PER_SET> Draws;
        std::array<RG::Resource, MAX_BUCKETS_PER_SET> DrawInfos;
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
