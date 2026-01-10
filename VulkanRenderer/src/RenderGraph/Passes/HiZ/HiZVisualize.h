#pragma once
#include "RenderGraph/RGGraph.h"

namespace Passes::HiZVisualize
{

struct ExecutionInfo
{
    RG::Resource Hiz{};
};

struct PassData
{
    RG::Resource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
