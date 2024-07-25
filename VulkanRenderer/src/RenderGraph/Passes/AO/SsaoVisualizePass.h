#pragma once
#include "RenderGraph/RenderGraph.h"

namespace Passes::SsaoVisualize
{
    struct PassData
    {
        RG::Resource SSAO{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource ssao, RG::Resource colorOut);
}
