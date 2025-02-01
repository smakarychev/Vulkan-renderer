#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::DiffuseIrradiance
{
    struct PassData
    {
        RG::Resource DiffuseIrradiance{};
        RG::Resource Cubemap{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, Texture cubemap,
        Texture irradiance);
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource cubemap,
        Texture irradiance);
}

