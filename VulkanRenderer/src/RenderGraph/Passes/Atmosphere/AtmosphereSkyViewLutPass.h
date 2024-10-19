#pragma once

#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::SkyView
{
    struct PassData
    {
        RG::Resource TransmittanceLut{};
        RG::Resource AtmosphereSettings{};
        RG::Resource DirectionalLight{};
        RG::Resource Camera{};
        RG::Resource Lut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph,
        RG::Resource transmittanceLut, RG::Resource atmosphereSettings, const SceneLight& light);
}
