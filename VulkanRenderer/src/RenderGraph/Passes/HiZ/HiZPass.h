#pragma once

#include "RenderGraph/RGResource.h"
#include "HiZCommon.h"

namespace Passes::HiZ
{
struct ExecutionInfo
{
    RG::ImageResource Depth{};
    ::HiZ::ReductionMode ReductionMode{::HiZ::ReductionMode::Min};
    bool CalculateMinMaxDepthBuffer{false};
};

struct PassData
{
    RG::BufferResource DepthMinMax{};
    RG::ImageResource HiZ{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
