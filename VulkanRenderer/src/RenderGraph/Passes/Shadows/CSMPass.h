#pragma once

#include "RenderGraph/RenderGraph.h"

struct ShadowPassExecutionInfo;

namespace Passes::CSM
{
    struct PassData
    {
        RG::Resource ShadowMap{};
        RG::Resource CSM{};
        f32 Near{1.0f};
        f32 Far{100.0f};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ShadowPassExecutionInfo& info);
}
