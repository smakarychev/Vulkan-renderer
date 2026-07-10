#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::ShapeNoise
{
struct ExecutionInfo
{
    f32 LowFrequencyTextureSize{128.0f};
    f32 HighFrequencyTextureSize{32.0f};
    /* optional external low frequency image */
    RG::ImageResource LowFrequencyTexture{};
    /* optional external high frequency image */
    RG::ImageResource HighFrequencyTexture{};
    RG::BufferResource LowFrequencyNoiseParameters{};
    RG::BufferResource HighFrequencyNoiseParameters{};
};

struct PassData
{
    RG::ImageResource LowFrequencyTexture{};
    RG::ImageResource HighFrequencyTexture{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
