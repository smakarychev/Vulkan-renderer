#pragma once

#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Culling/CullingTraits.h"

class CullMultiviewData;

namespace RG
{
    struct CullMultiviewResources;
}

struct MeshletCullMultiviewPassExecutionInfo
{
    RG::CullMultiviewResources* MultiviewResource{nullptr};
};

namespace Passes::Multiview::MeshletCullTranslucent
{
    struct PassData
    {
        RG::CullMultiviewResources* MultiviewResource{nullptr};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        const MeshletCullMultiviewPassExecutionInfo& info);
}
