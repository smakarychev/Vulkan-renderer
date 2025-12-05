#pragma once

#include "ChannelCompositionInfo.h"
#include "RenderGraph/RGResource.h"

namespace Passes::Texture2dToTexture2d
{
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn, 
    ChannelComposition channelComposition = ChannelComposition{});
}
namespace Passes::Texture3dToSlice
{
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn, f32 sliceNormalized,
    ChannelComposition channelComposition = ChannelComposition{});
}
namespace Passes::TextureArrayToSlice
{
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn, u32 slice,
    ChannelComposition channelComposition = ChannelComposition{});
}
namespace Passes::TextureArrayToAtlas
{
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    ChannelComposition channelComposition = ChannelComposition{});
}
