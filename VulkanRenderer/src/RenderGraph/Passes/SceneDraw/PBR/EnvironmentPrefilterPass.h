#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::EnvironmentPrefilter
{
struct PassData
{
    RG::ImageResource Cubemap{};
    RG::ImageResource PrefilteredTexture{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource cubemap,
    RG::ImageResource prefiltered, bool realTime);

TextureDescription getPrefilteredTextureDescription(u32 resolution);
}
