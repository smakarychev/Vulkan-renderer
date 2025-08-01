#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds
{
    struct CloudsNoiseParameters;
}

namespace Passes::Clouds::VP::Coverage
{
    struct ExecutionInfo
    {
        /* optional external coverage map image */
        Image CoverageMap{};
        const CloudsNoiseParameters* NoiseParameters{nullptr};
    };
    struct PassData
    {
        RG::Resource CoverageMap{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

