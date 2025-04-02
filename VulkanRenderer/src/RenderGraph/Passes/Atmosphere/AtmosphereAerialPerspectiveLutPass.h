#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::AerialPerspective
{
    struct PassData
    {
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        RG::Resource AtmosphereSettings{};
        RG::Resource DirectionalLight{};
        RG::Resource Camera{};
        RG::CSMData CSMData{};
        RG::Resource Lut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        RG::Resource transmittanceLut, RG::Resource multiscatteringLut,
        RG::Resource atmosphereSettings, const SceneLight& light, const RG::CSMData& csmData);
}

