#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::EnvironmentPrefilter
{
    struct PassData
    {
        RG::Resource Cubemap{};
        RG::Resource PrefilteredTexture{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, Texture cubemap,
        Texture prefiltered);
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource cubemap,
        Texture prefiltered);

    TextureDescription getPrefilteredTextureDescription();
}

