#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersDepthLayersVisualize
{
    struct PassData
    {
        RG::Resource Depth{};
        RG::Resource ColorOut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource depth);
}
