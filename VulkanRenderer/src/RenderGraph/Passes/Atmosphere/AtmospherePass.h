#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::LutPasses
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
};

struct PassData
{
    RG::ImageResource TransmittanceLut{};
    RG::ImageResource MultiscatteringLut{};
    RG::ImageResource SkyViewLut{};
    RG::BufferResource ViewInfo{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
