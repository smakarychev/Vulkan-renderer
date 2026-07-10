#pragma once
#include "RenderGraph/RGResource.h"

struct ZBins;

namespace Passes::LightTilesVisualize
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::BufferResource Tiles{};
    RG::BufferResource Bins{};
    RG::ImageResource Depth{};
};

struct PassData
{
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
