#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::EnvironmentPrefilter
{
    struct PassData
    {
        RG::Resource Cubemap{};
        RG::Resource PrefilteredTexture{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, Texture cubemap,
        Texture prefiltered);
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource cubemap,
        Texture prefiltered);

    TextureDescription getPrefilteredTextureDescription();
}

