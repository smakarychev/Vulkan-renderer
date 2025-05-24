#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::HiZFull
{
    struct ExecutionInfo
    {
        RG::Resource Depth{};
    };
    struct PassData
    {
        RG::Resource MinMaxDepth{};
        RG::Resource Depth{};
        RG::Resource HiZMin{};
        RG::Resource HiZMax{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
