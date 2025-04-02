#pragma once
#include "CullMultiviewResources.h"
#include "RenderGraph/Passes/Culling/CullingTraits.h"

struct MeshCullMultiviewPassExecutionInfo
{
    RG::CullMultiviewResources* MultiviewResource{nullptr};
};

namespace Passes::Multiview::MeshCull
{
    struct PassData
    {
        RG::CullMultiviewResources* MultiviewResource{nullptr};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const MeshCullMultiviewPassExecutionInfo& info,
        CullStage stage);
}
