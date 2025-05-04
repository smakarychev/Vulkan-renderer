#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Fxaa
{
    struct PassData
    {
        RG::Resource ColorIn;
        RG::Resource AntiAliased;
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource colorIn);
}
