#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::DiffuseIrradianceSH
{
    struct PassData
    {
        RG::Resource DiffuseIrradiance{};
        RG::Resource CubemapTexture{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, Texture cubemap,
        Buffer irradianceSH, bool realTime);
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource cubemap,
        Buffer irradianceSH, bool realTime);
}
