#pragma once

#include "RenderGraph/RGGraph.h"

namespace Passes::Ssao
{
struct ExecutionInfo
{
    RG::ImageResource Depth{};
    u32 MaxSampleCount{32};
};

struct PassData
{
    RG::ImageResource Ssao{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
