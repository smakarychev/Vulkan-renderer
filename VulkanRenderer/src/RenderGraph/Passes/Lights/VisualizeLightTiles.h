#pragma once
#include "RenderGraph/RGResource.h"

struct ZBins;

namespace Passes::LightTilesVisualize
{
    struct PassData
    {
        RG::Resource Tiles{};
        RG::Resource Camera{};
        RG::Resource ColorOut{};
        RG::Resource ZBins{};
        RG::Resource Depth{};
    };
    
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource tiles, RG::Resource depth,
        RG::Resource bins);
}
