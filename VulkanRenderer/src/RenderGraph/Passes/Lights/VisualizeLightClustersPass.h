#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersVisualize
{
    struct PassData
    {
        RG::Resource Clusters{};
        RG::Resource Camera{};
        RG::Resource ColorOut{};
        RG::Resource Depth{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource depth, RG::Resource clusters);
}

