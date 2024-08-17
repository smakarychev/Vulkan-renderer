#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersCompact
{
    struct PassData
    {
        RG::Resource Clusters;
        RG::Resource ActiveClusters;
        RG::Resource ActiveClustersCount;
        RG::Resource ClustersVisibility;
        RG::Resource Depth;
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource clusters, RG::Resource& depth);
}
