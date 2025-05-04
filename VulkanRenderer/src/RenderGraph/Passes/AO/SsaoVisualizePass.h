#pragma once
#include "RenderGraph/RenderGraph.h"

namespace Passes::SsaoVisualize
{
    struct PassData
    {
        RG::Resource Color{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource ssao);
}
