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
        RG::Resource TransmittanceLut;
        RG::Resource ColorOut;
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource transmittanceLut);
}
