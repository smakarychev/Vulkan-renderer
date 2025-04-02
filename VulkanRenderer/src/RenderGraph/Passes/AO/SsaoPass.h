#pragma once

#include "RenderGraph/RenderGraph.h"

namespace Passes::Ssao
{
    struct PassData
    {
        RG::Resource DepthIn{};
        RG::Resource SSAO{};

        RG::Resource NoiseTexture{};
        RG::Resource Settings{};
        RG::Resource Camera{};
        RG::Resource Samples{};

        u32 MaxSampleCount{0};
    };

    RG::Pass& addToGraph(StringId name, u32 sampleCount, RG::Graph& renderGraph, RG::Resource depthIn);
}
