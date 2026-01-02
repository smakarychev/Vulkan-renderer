#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::LightTilesSetup
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
};
struct PassData
{
    RG::Resource Tiles{};
};
PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
