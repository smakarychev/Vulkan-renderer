#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::Transmittance
{
struct PassData
{
    RG::Resource Lut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource viewInfo);
}
