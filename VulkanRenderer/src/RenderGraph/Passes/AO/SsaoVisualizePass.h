#pragma once
#include "RenderGraph/RenderGraph.h"

namespace Passes::SsaoVisualize
{
    struct PassData
    {
        RG::Resource SSAO{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource ssao, RG::Resource colorOut);
}
