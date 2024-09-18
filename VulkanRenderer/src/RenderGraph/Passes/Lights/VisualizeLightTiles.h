#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightTilesVisualize
{
    struct PassData
    {
        RG::Resource Tiles{};
        RG::Resource Camera{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource tiles);
}
