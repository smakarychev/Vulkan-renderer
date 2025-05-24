#pragma once
#include "RenderGraph/RGResource.h"

class Camera;

/* note: the readback has a delay of `BUFFERED_FRAMES` frames */
namespace Passes::DepthReductionReadback
{
    struct ExecutionInfo
    {
        RG::Resource MinMaxDepthReduction{};
        const Camera* PrimaryCamera{nullptr};
    };
    struct PassData
    {
        RG::Resource MinMaxDepth;
        f32 Min{1};
        f32 Max{0};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

