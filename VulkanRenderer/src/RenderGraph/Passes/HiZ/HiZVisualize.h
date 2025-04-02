#pragma once
#include "RenderGraph/RenderGraph.h"

namespace Passes::HiZVisualize
{
    struct PassData
    {
        RG::Resource HiZ;
        RG::Resource ColorOut;
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource hiz);
}
