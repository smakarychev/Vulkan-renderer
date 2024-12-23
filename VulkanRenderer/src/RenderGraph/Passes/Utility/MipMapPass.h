#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Mipmap
{
    struct PassData
    {
        RG::Resource Texture{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource texture);
}

