#pragma once

#include "RenderGraph/RGResource.h"
#include "RenderGraph/RGCommon.h"

namespace Passes::Crt
{
    struct PassData
    {
        RG::Resource ColorIn;
        RG::Resource ColorOut{};
        RG::Resource Time{};
        RG::Resource Settings{};
    };
    
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource colorIn, RG::Resource colorTarget);
}
