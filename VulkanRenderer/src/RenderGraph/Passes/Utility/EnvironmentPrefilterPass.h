#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::EnvironmentPrefilter
{
    struct PassData
    {
        RG::Resource Cubemap;
        RG::Resource PrefilteredTexture;
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& cubemap,
        const Texture& prefiltered);
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource cubemap,
        const Texture& prefiltered);

    TextureDescription getPrefilteredTextureDescription();
}

