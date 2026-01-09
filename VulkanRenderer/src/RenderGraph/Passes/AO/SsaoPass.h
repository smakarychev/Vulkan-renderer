#pragma once

#include "RenderGraph/RGGraph.h"

namespace Passes::Ssao
{
struct ExecutionInfo
{
    RG::Resource Depth{};
    u32 MaxSampleCount{32};
};

struct PassData
{
    RG::Resource Ssao{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
