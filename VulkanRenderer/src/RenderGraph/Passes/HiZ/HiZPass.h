#pragma once

#include "RenderGraph/RGResource.h"
#include "HiZCommon.h"

namespace Passes::HiZ
{
    struct ExecutionInfo
    {
        RG::Resource Depth{};
        ::HiZ::ReductionMode ReductionMode{::HiZ::ReductionMode::Min};
        bool CalculateMinMaxDepthBuffer{false};
    };
    struct PassData
    {
        RG::Resource DepthMinMax{};
        RG::Resource HiZ{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
