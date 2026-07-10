#pragma once
#include "RenderGraph/RGDrawResources.h"

class SceneLight;

namespace Passes::LightTilesBin
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::BufferResource Tiles{};
    RG::ImageResource Depth{};
    RG::BufferResource PointLights{};
};

struct PassData
{
    RG::BufferResource Tiles{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
