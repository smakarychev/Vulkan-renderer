#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::EquirectangularToCubemap
{
    struct PassData
    {
        RG::Resource Cubemap{};
        RG::Resource Equirectangular{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, Texture equirectangular,
        Texture cubemap);
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource equirectangular,
        Texture cubemap);
}

