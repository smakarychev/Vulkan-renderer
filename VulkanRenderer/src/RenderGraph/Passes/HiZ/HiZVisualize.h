#pragma once
#include "RenderGraph/RGGraph.h"

namespace Passes::HiZVisualize
{
struct PassData
{
    RG::Resource HiZ{};
    RG::Resource ColorOut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource hiz);
}
