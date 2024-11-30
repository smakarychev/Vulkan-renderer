#pragma once
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::Raymarch
{
    struct PassData
    {
        RG::Resource DepthIn{};
        RG::Resource AtmosphereSettings{};
        RG::Resource SkyViewLut{};
        RG::Resource TransmittanceLut{};
        RG::Resource AerialPerspectiveLut{};
        RG::Resource Camera{};
        RG::Resource DirectionalLight{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph,
        RG::Resource atmosphereSettings, const SceneLight& light,
        RG::Resource skyViewLut, RG::Resource transmittanceLut, RG::Resource aerialPerspectiveLut,
        RG::Resource colorIn, RG::Resource depthIn, bool useSunLuminance);
}

