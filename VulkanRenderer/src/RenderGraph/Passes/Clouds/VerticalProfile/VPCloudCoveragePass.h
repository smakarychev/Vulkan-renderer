#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::VP::Coverage
{
struct ExecutionInfo
{
    /* optional external coverage map image */
    Image CoverageMap{};
    RG::Resource NoiseParameters{};
};

struct PassData
{
    RG::Resource CoverageMap{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
