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
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource renderTarget);
}
