#pragma once
#include "RenderGraph/RGDrawResources.h"

class SceneLight;

namespace Passes::LightTilesBin
{
    struct ExecutionInfo
    {
        RG::Resource Tiles{};
        RG::Resource Depth{};
        const SceneLight* Light{nullptr};
    };
    struct PassData
    {
        RG::Resource Tiles{};
        RG::Resource Depth{};
        RG::Resource Camera{};
        RG::SceneLightResources SceneLightResources{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

