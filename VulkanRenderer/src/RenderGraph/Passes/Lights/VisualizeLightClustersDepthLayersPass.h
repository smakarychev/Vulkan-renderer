#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersDepthLayersVisualize
{
    struct PassData
    {
        RG::Resource Depth{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource depth);
}
