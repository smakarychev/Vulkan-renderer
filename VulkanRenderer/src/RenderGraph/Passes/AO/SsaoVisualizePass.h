#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

class SsaoVisualizePass
{
public:
    struct PassData
    {
        RenderGraph::Resource SSAO{};
        RenderGraph::Resource ColorOut{};

        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    SsaoVisualizePass(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource ssao, RenderGraph::Resource colorOut);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::PipelineData m_PipelineData{};
};
