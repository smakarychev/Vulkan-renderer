#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::LightClustersVisualize
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::BufferResource Clusters{};
    RG::ImageResource Depth{};
};

struct PassData
{
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
