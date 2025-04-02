#pragma once
#include "CullMultiviewResources.h"

struct TriangleCullPrepareMultiviewPassExecutionInfo
{
    RG::CullTrianglesMultiviewResource* MultiviewResource{nullptr};
};

struct TriangleCullMultiviewPassExecutionInfo
{
    RG::CullTrianglesMultiviewResource* MultiviewResource{nullptr};
};

namespace Passes::Multiview::TrianglePrepareCull
{
    struct PassData
    {
        RG::CullTrianglesMultiviewResource* MultiviewResource{nullptr};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        const TriangleCullPrepareMultiviewPassExecutionInfo& info);
}

namespace Passes::Multiview::TriangleCull
{
    struct PassData
    {
        std::vector<RG::DrawAttachmentResources> DrawAttachmentResources{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        const TriangleCullMultiviewPassExecutionInfo& info, CullStage stage);
}