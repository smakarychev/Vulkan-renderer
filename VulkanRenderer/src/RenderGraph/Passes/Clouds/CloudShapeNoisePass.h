#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::ShapeNoise
{
struct ExecutionInfo
{
    f32 LowFrequencyTextureSize{128.0f};
    f32 HighFrequencyTextureSize{32.0f};
    /* optional external low frequency image */
    Image LowFrequencyTexture{};
    /* optional external high frequency image */
    Image HighFrequencyTexture{};
    RG::Resource LowFrequencyNoiseParameters{};
    RG::Resource HighFrequencyNoiseParameters{};
};

struct PassData
{
    RG::Resource LowFrequencyTexture{};
    RG::Resource HighFrequencyTexture{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
