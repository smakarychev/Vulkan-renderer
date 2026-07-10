#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersCascadesVisualize
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::ImageResource Depth{};
};
struct PassData
{
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
