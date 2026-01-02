#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::Multiscattering
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource TransmittanceLut{};
};

struct PassData
{
    RG::Resource Lut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
