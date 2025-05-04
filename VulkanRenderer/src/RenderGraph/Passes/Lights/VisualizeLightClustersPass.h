#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersVisualize
{
    struct ExecutionInfo
    {
        RG::Resource Clusters{};
        RG::Resource Depth{};
    };
    struct PassData
    {
        RG::Resource Color{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

