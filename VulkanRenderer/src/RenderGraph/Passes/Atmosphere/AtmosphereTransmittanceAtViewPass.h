#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::AtmosphereUpdateSunParameters
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::ImageResource TransmittanceLut{};
};

struct PassData
{
    RG::BufferResource ViewInfo{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
