#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::Multiscattering
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::ImageResource TransmittanceLut{};
};

struct PassData
{
    RG::ImageResource Lut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
