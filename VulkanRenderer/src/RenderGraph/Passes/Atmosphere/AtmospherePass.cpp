#include "AtmospherePass.h"

AtmosphereSettings AtmosphereSettings::EarthDefault()
{
    return {
        .Surface = 6.360f,
        .Atmosphere = 6.460f,
        .RayleighScattering = glm::vec3{5.802f, 13.558f, 33.1f},
        .RayleighAbsorption = glm::vec3{0.0f},
        .MieScattering = glm::vec3{3.996f},
        .MieAbsorption = glm::vec3{4.4f},
        .OzoneAbsorption = glm::vec3{0.650f, 1.881f, 0.085f},
        .RayleighDensity = 1.0f,
        .MieDensity = 1.0f,
        .OzoneDensity = 1.0f};
}

RG::Pass& Passes::Atmosphere::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const AtmosphereSettings& atmosphereSettings)
{
    
}
