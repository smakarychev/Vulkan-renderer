#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::DiffuseIrradiance
{
    struct PassData
    {
        RG::Resource DiffuseIrradiance{};
        RG::Resource Cubemap{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, Texture cubemap,
        Texture irradiance);
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource cubemap,
        Texture irradiance);
}

