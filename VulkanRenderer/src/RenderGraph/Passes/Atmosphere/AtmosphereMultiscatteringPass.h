#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::Multiscattering
{
    struct PassData
    {
        RG::Resource TransmittanceLut{};
        RG::Resource AtmosphereSettings{};
        RG::Resource Lut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        RG::Resource transmittanceLut, RG::Resource atmosphereSettings);
}
