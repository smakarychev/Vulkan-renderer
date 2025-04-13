#pragma once

#include "RenderGraph/RGResource.h"

namespace HiZ
{
    enum class ReductionMode
    {
        Min,
        Max
    };

    struct MinMaxDepth
    {
        u32 Min{std::bit_cast<u32>(1.0f)};
        u32 Max{std::bit_cast<u32>(0.0f)};
    };
    
    Sampler createSampler(ReductionMode mode);
    RG::Resource createHiz(RG::Graph& renderGraph, const glm::uvec2& depthResolution);
    RG::Resource createMinMaxBuffer(RG::Graph& renderGraph);
}
