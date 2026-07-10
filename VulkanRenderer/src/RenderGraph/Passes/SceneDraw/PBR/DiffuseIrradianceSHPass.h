#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::DiffuseIrradianceSH
{
struct PassData
{
    RG::BufferResource DiffuseIrradiance{};
    RG::ImageResource CubemapTexture{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource cubemap,
    RG::BufferResource irradianceSH, bool realTime);
}
