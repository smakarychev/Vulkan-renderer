#pragma once

#include "ChannelCompositionInfo.h"
#include "RenderGraph/RGResource.h"

namespace Passes::Texture2dToTexture2d
{
RG::ImageResource addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn, 
    ChannelComposition channelComposition = ChannelComposition{});
}
namespace Passes::Texture3dToSlice
{
RG::ImageResource addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn, f32 sliceNormalized,
    ChannelComposition channelComposition = ChannelComposition{});
}
namespace Passes::TextureArrayToSlice
{
RG::ImageResource addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn, u32 slice,
    ChannelComposition channelComposition = ChannelComposition{});
}
namespace Passes::TextureArrayToAtlas
{
RG::ImageResource addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn,
    ChannelComposition channelComposition = ChannelComposition{});
}
