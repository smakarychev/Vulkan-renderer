#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"

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
        RenderGraph::Resource HiZ;
        RenderGraph::Resource ColorOut;

        RenderGraph::PipelineData* PipelineData{nullptr};
        
        PushConstants* PushConstants{nullptr};
    };
public:
    HiZVisualize(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource hiz);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::PipelineData m_PipelineData;
    PushConstants m_PushConstants{};
};
