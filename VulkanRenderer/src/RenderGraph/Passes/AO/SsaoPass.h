#pragma once

#include "RenderGraph/RenderGraph.h"

namespace Passes::Ssao
{
    struct ExecutionInfo
    {
        RG::Resource Depth{};
        u32 MaxSampleCount{32};
    };
    struct PassData
    {
        RG::Resource SSAO{};
    };

    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
