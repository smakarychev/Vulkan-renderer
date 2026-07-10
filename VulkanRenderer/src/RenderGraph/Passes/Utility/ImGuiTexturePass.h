#pragma once

#include "ChannelCompositionInfo.h"
#include "RenderGraph/RGPass.h"

namespace Passes::ImGuiTexture
{
RG::ImageResource addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn);
RG::ImageResource addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn,
    ChannelComposition channelComposition);
}

namespace Passes::ImGuiCubeTexture
{
RG::ImageResource addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn);
}

namespace Passes::ImGuiArrayTexture
{
enum class DrawAs : u8 {Slice, Atlas};
RG::ImageResource addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn, DrawAs drawAs, 
    ChannelComposition channelComposition = ChannelComposition{});
}

namespace Passes::ImGuiTexture3d
{
RG::ImageResource addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn,
    ChannelComposition channelComposition = ChannelComposition{});
}