#pragma once
#include "RenderGraph/RGGraph.h"

namespace Passes::SsaoVisualize
{
    struct PassData
    {
        RG::Resource Color{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource ssao);
}
