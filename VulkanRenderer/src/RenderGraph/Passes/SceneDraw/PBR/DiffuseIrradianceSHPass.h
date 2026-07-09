#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::DiffuseIrradianceSH
{
struct PassData
{
    RG::Resource DiffuseIrradiance{};
    RG::Resource CubemapTexture{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource cubemap,
    RG::Resource irradianceSH, bool realTime);
}
