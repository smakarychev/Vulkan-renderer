#pragma once

#include "ViewInfoGPU.h"
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace RG
{
    struct CsmData;
}

namespace Passes::Atmosphere::LutPasses
{
    struct ExecutionInfo
    {
        RG::Resource ViewInfo{};
        const SceneLight* Light{nullptr};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource TransmittanceLut{};
        RG::Resource MultiscatteringLut{};
        RG::Resource SkyViewLut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

