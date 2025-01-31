#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::DiffuseIrradianceSH
{
    struct PassData
    {
        RG::Resource DiffuseIrradiance{};
        RG::Resource CubemapTexture{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& cubemap,
        Buffer irradianceSH, bool realTime);
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource cubemap,
        Buffer irradianceSH, bool realTime);
}
