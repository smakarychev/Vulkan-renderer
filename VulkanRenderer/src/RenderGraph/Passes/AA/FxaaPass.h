#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Fxaa
{
    struct PassData
    {
        RG::Resource ColorIn;
        RG::Resource AntiAliased;
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource colorIn);
}
