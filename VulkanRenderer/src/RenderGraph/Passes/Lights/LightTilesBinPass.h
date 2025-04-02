#pragma once
#include "RenderGraph/RGDrawResources.h"

namespace Passes::LightTilesBin
{
    struct PassData
    {
        RG::Resource Tiles{};
        RG::Resource Depth{};
        RG::Resource Camera{};
        RG::SceneLightResources SceneLightResources{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource tiles, RG::Resource depth,
        const SceneLight& sceneLight);
}

