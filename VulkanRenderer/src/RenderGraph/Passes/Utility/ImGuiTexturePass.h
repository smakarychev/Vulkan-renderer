#pragma once

#include "ChannelCompositionInfo.h"
#include "RenderGraph/RGPass.h"

namespace Passes::ImGuiTexture
{
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, Texture texture);
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn);
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, Texture texture,
    ChannelComposition channelComposition);
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    ChannelComposition channelComposition);
}

namespace Passes::ImGuiCubeTexture
{
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, Texture texture);
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn);
}

namespace Passes::ImGuiArrayTexture
{
enum class DrawAs : u8 {Slice, Atlas};
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, Texture texture, DrawAs drawAs,
    ChannelComposition channelComposition = ChannelComposition{});
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn, DrawAs drawAs, 
    ChannelComposition channelComposition = ChannelComposition{});
}

namespace Passes::ImGuiTexture3d
{
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, Texture texture,
    ChannelComposition channelComposition = ChannelComposition{});
RG::Resource addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn,
    ChannelComposition channelComposition = ChannelComposition{});
}