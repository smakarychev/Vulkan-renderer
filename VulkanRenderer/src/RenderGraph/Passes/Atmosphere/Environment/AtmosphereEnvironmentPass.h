#pragma once
#include "RenderGraph/RGResource.h"

class SceneLight;

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
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph,
        RG::Resource atmosphereSettings, const SceneLight& light, RG::Resource skyViewLut);
}
