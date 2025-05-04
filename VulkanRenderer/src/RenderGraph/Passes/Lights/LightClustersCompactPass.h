#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersCompact
{
    struct ExecutionInfo
    {
        RG::Resource Clusters{};
        RG::Resource ClusterVisibility{};
        RG::Resource Depth{};
    };
    struct PassData
    {
        RG::Resource Clusters{};
        RG::Resource ActiveClusters{};
        RG::Resource ActiveClustersCount{};
        RG::Resource ClusterVisibility{};
        RG::Resource Depth{};
        RG::Resource DispatchIndirect{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
