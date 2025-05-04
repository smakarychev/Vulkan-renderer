#pragma once

#include "HiZCommon.h"
#include "RenderGraph/RGResource.h"

namespace Passes::HiZNV
{
    struct ExecutionInfo
    {
        RG::Resource Depth{};
        ImageSubresourceDescription Subresource{};
        HiZ::ReductionMode ReductionMode{HiZ::ReductionMode::Min};
    };
    struct PassData
    {
        RG::Resource Depth{};
        RG::Resource HiZ{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

