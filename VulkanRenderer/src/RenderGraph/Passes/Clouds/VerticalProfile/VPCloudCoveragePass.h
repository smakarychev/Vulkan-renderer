#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::VP::Coverage
{
struct ExecutionInfo
{
    /* optional external coverage map image */
    RG::ImageResource CoverageMap{};
    RG::BufferResource NoiseParameters{};
};

struct PassData
{
    RG::ImageResource CoverageMap{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
