#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::EquirectangularToCubemap
{
    struct PassData
    {
        RG::Resource Cubemap{};
        RG::Resource Equirectangular{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& equirectangular,
        const Texture& cubemap);
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource equirectangular,
        const Texture& cubemap);
}

