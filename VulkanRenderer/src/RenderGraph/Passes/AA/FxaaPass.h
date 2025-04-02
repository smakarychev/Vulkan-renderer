#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Fxaa
{
    struct PassData
    {
        RG::Resource ColorIn;
        RG::Resource AntiAliased;
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource colorIn);
}
