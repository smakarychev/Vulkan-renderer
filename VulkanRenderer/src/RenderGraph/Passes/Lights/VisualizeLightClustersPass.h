#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::VisualizeLightClusters
{
    struct PassData
    {
        RG::Resource Clusters{};
        RG::Resource Camera{};
        RG::Resource ColorOut{};
        RG::Resource Depth{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth, RG::Resource clusters);
}

