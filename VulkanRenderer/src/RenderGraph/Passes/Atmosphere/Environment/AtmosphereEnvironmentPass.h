#pragma once
#include "RenderGraph/RGResource.h"

class SceneLight2;

namespace Passes::Atmosphere::Environment
{
    struct PassData
    {
        RG::Resource AtmosphereSettings{};
        RG::Resource SkyViewLut{};
        RG::Resource Camera{};
        RG::Resource DirectionalLight{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        RG::Resource atmosphereSettings, const SceneLight2& light, RG::Resource skyViewLut);
}

