#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::Transmittance
{
    struct PassData
    {
        RG::Resource Lut{};
        RG::Resource AtmosphereSettings{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource atmosphereSettings);
}


