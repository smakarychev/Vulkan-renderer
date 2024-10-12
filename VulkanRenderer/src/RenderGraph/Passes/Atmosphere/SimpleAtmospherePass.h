#pragma once
#include "RenderGraph/RGResource.h"

namespace RG
{
    class Pass;
}

namespace Passes::AtmosphereSimple
{
    struct PassData
    {
        RG::Resource Camera;
        RG::Resource ColorOut;
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph);
}
