#pragma once
#include "RenderGraph/RenderGraph.h"

class HiZVisualize
{
public:
    struct PushConstants
    {
        u32 MipLevel{0};
        f32 IntensityScale{10.0f};
    };
    struct PassData
    {
        RG::Resource HiZ;
        RG::Resource ColorOut;

        PushConstants* PushConstants{nullptr};
    };
public:
    HiZVisualize(RG::Graph& renderGraph);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource hiz);
private:
    RG::Pass* m_Pass{nullptr};

    PushConstants m_PushConstants{};
};
