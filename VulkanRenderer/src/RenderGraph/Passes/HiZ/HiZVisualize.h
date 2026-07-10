#pragma once
#include "RenderGraph/RGGraph.h"

namespace Passes::HiZVisualize
{

struct ExecutionInfo
{
    RG::ImageResource Hiz{};
};

struct PassData
{
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
