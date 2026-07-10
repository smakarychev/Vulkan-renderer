#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::Transmittance
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
};

struct PassData
{
    RG::ImageResource Lut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
