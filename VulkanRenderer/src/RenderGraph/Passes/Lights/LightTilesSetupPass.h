#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::LightTilesSetup
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
};

struct PassData
{
    RG::BufferResource Tiles{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
