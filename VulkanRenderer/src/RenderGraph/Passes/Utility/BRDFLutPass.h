#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::BRDFLut
{
    struct PassData
    {
        RG::Resource Lut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& lut);
    
    TextureDescription getLutDescription();
}

