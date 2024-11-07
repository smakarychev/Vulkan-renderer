#pragma once
#include "RenderGraph/RGResource.h"

namespace RG
{
    struct CSMData;
}

class SceneLight;

struct AtmosphereSettings
{
    glm::vec4 RayleighScattering{};
    glm::vec4 RayleighAbsorption{};
    glm::vec4 MieScattering{};
    glm::vec4 MieAbsorption{};
    glm::vec4 OzoneAbsorption{};
    glm::vec4 SurfaceAlbedo{};
    
    f32 Surface{};
    f32 Atmosphere{};
    f32 RayleighDensity{};
    f32 MieDensity{};
    f32 OzoneDensity{};


    static AtmosphereSettings EarthDefault();
};

namespace Passes::Atmosphere
{
    struct PassData
    {
        RG::Resource DepthIn{};
        RG::Resource AtmosphereSettings{};
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        RG::Resource SkyViewLut{};
        RG::Resource AerialPerspectiveLut{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const AtmosphereSettings& atmosphereSettings,
        const SceneLight& light, RG::Resource colorIn, RG::Resource depthIn, const RG::CSMData& csmData);
}

