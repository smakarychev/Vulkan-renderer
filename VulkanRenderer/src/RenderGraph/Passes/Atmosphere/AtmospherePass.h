#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::LutPasses
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
};

struct PassData
{
    RG::Resource TransmittanceLut{};
    RG::Resource MultiscatteringLut{};
    RG::Resource SkyViewLut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
