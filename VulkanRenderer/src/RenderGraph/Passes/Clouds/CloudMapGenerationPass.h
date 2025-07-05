#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds
{
    struct CloudsNoiseParameters;
}

namespace Passes::CloudMapGeneration
{
    struct ExecutionInfo
    {
        /* optional external cloud map image */
        Image CloudMap{};
        const Clouds::CloudsNoiseParameters* NoiseParameters{nullptr};
    };
    struct PassData
    {
        RG::Resource CloudMap{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
