#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

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

        RG::PipelineData* PipelineData{nullptr};
        
        PushConstants* PushConstants{nullptr};
    };
public:
    HiZVisualize(RG::Graph& renderGraph);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource hiz);
private:
    RG::Pass* m_Pass{nullptr};

    RG::PipelineData m_PipelineData;
    PushConstants m_PushConstants{};
};
