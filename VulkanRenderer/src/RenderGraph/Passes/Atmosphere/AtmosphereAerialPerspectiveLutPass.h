#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::AerialPerspective
{
    struct ExecutionInfo
    {
        RG::Resource ViewInfo{};
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        const SceneLight* Light{nullptr};
        RG::CsmData CsmData{};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        RG::Resource DirectionalLight{};
        RG::CsmData CsmData{};
        RG::Resource AerialPerspective{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

