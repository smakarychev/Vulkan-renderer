#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Mipmap
{
    struct PassData
    {
        RG::Resource Texture{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource texture);
}

