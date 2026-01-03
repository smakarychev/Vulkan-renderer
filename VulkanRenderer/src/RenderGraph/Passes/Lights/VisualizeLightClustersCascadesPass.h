#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersCascadesVisualize
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource Depth{};
};
struct PassData
{
    RG::Resource Depth{};
    RG::Resource ColorOut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
