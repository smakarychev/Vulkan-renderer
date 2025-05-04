#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Mipmap
{
    struct PassData
    {
        RG::Resource Texture{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource texture);
}

