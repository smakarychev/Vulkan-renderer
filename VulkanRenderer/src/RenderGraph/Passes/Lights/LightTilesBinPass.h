#pragma once
#include "RenderGraph/RGDrawResources.h"

class SceneLight;

namespace Passes::LightTilesBin
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource Tiles{};
    RG::Resource Depth{};
    RG::Resource PointLights{};
};

struct PassData
{
    RG::Resource Tiles{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
