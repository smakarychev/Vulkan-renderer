#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersDepthLayersVisualize
{
    struct PassData
    {
        RG::Resource Depth{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth);
}
