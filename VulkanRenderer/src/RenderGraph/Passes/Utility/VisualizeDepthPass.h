#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::VisualizeDepth
{
    struct PassData
    {
        RG::Resource DepthIn{};
        RG::Resource ColorOut{};
    };

    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource depthIn, RG::Resource colorIn,
        f32 near, f32 far, bool isOrthographic);
}
