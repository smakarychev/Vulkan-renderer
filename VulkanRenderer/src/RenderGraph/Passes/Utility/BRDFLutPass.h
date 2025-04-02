#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::BRDFLut
{
    struct PassData
    {
        RG::Resource Lut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, Texture lut);
    
    TextureDescription getLutDescription();
}

