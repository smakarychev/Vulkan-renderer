#pragma once
#include "RenderGraph/RenderGraph.h"

namespace Passes::SkyGradient
{
    struct PassData
    {
        RG::Resource ColorOut;
        RG::Resource Camera;
        RG::Resource Settings;
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource renderTarget);
}
