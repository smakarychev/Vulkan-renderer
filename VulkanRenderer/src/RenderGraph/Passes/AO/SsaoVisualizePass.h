#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

class SsaoVisualizePass
{
public:
    struct PassData
    {
        RG::Resource SSAO{};
        RG::Resource ColorOut{};

        RG::PipelineData* PipelineData{nullptr};
    };
public:
    SsaoVisualizePass(RG::Graph& renderGraph);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource ssao, RG::Resource colorOut);
private:
    RG::Pass* m_Pass{nullptr};

    RG::PipelineData m_PipelineData{};
};
