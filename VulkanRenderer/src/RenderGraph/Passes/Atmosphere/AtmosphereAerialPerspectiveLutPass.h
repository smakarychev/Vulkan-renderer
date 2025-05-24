#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::AerialPerspective
{
    struct ExecutionInfo
    {
        RG::Resource AtmosphereSettings{};
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        const SceneLight* SceneLight{nullptr};
        RG::CsmData CsmData{};
    };
    struct PassData
    {
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        RG::Resource AtmosphereSettings{};
        RG::Resource DirectionalLight{};
        RG::Resource Camera{};
        RG::CsmData CsmData{};
        RG::Resource AerialPerspective{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

