#pragma once
#include "RenderGraph/RGResource.h"

class Camera;

/* note: the readback has a delay of `BUFFERED_FRAMES` frames */
namespace Passes::DepthReductionReadback
{
    struct PassData
    {
        RG::Resource MinMaxDepth;
        f32 Min{1};
        f32 Max{0};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource minMaxDepth,
        const Camera* primaryCamera);
}

