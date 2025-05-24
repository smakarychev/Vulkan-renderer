#pragma once
#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace RG
{
    struct CsmData;
}

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

namespace Passes::Atmosphere::LutPasses
{
    struct ExecutionInfo
    {
        const AtmosphereSettings* AtmosphereSettings{nullptr};
        const SceneLight* SceneLight{nullptr};
    };
    struct PassData
    {
        RG::Resource AtmosphereSettings{};
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        RG::Resource SkyViewLut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

