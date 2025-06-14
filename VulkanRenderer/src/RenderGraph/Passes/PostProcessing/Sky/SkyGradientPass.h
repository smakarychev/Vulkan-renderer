#pragma once
#include "RenderGraph/RGGraph.h"

namespace Passes::SkyGradient
{
    struct PassData
    {
        RG::Resource ColorOut;
        RG::Resource ViewInfo;
        RG::Resource Settings;
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource renderTarget);
}
