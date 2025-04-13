#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::HiZFull
{
    struct ExecutionInfo
    {
        RG::Resource Depth{};
        ImageSubresourceDescription Subresource{};
    };
    struct PassData
    {
        RG::Resource MinMaxDepth{};
        RG::Resource Depth{};
        RG::Resource HiZMin{};
        RG::Resource HiZMax{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
