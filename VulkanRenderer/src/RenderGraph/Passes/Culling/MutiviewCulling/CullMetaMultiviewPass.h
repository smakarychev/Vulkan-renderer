#pragma once

#include "TriangleCullMultiviewPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGDrawResources.h"

class CullMultiviewData;

struct CullMetaMultiviewPassInitInfo
{
    CullMultiviewData* MultiviewData{nullptr};
};

namespace Passes::Meta::CullMultiview
{
    struct PassData
    {
        std::vector<RG::DrawAttachmentResources> DrawAttachmentResources{};
        std::vector<RG::Resource> HiZOut{};
        RG::Resource HiZMaxOut{};
        RG::Resource MinMaxDepth{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, CullMultiviewData& multiviewData);
}
