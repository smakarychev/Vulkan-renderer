#pragma once

#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::Atmosphere::SkyView
{
    struct ExecutionInfo
    {
        RG::Resource ViewInfo{};
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        const SceneLight* Light{nullptr};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        RG::Resource DirectionalLight{};
        RG::Resource Lut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
