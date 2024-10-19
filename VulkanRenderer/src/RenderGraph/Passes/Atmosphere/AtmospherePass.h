#pragma once
#include "RenderGraph/RGResource.h"

class SceneLight;

struct AtmosphereSettings
{
    f32 Surface{};
    f32 Atmosphere{};
    
    glm::vec3 RayleighScattering{};
    glm::vec3 RayleighAbsorption{};
    glm::vec3 MieScattering{};
    glm::vec3 MieAbsorption{};
    glm::vec3 OzoneAbsorption{};
    
    f32 RayleighDensity{};
    f32 MieDensity{};
    f32 OzoneDensity{};

    static AtmosphereSettings EarthDefault();
};

namespace Passes::Atmosphere
{
    struct PassData
    {
        RG::Resource AtmosphereSettings{};
        RG::Resource TransmittanceLut{};
        RG::Resource SkyViewLut{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const AtmosphereSettings& atmosphereSettings,
        const SceneLight& light);
}

