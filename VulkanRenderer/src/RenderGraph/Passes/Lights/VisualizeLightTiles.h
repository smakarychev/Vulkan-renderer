#pragma once
#include "RenderGraph/RGResource.h"

struct ZBins;

namespace Passes::LightTilesVisualize
{
    struct ExecutionInfo
    {
        RG::Resource Tiles{};
        RG::Resource Bins{};
        RG::Resource Depth{};
    };
    struct PassData
    {
        RG::Resource Color{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
