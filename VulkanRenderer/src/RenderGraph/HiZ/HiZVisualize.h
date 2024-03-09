#pragma once
#include "RenderGraph/RenderGraph.h"

class HiZVisualize
{
public:
    struct PassData
    {
        RenderGraph::Resource HiZ;
        RenderGraph::Resource ColorOut;

        struct PushConstants
        {
            u32 MipLevel{0};
            f32 IntensityScale{1.0f};
        };
        PushConstants PushConstants{};
    };
public:
    HiZVisualize(RenderGraph::Graph& renderGraph, RenderGraph::Resource hiz);
private:
    RenderGraph::Pass* m_Pass{nullptr};
};
