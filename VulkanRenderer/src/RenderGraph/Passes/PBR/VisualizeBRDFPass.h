#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

class VisualizeBRDFPass
{
public:
    struct PassData
    {
        RenderGraph::Resource BRDF{};
        RenderGraph::Resource ColorOut{};

        Sampler BRDFSampler{};
        
        RenderGraph::PipelineData* PipelineData{nullptr};    
    };
public:
    VisualizeBRDFPass(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, const Texture& brdf, RenderGraph::Resource colorIn,
        const glm::uvec2 resolution);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::PipelineData m_PipelineData{};
};
