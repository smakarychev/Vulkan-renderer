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
    
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource tiles, RG::Resource depth,
        std::optional<ZBins> bins);
}
