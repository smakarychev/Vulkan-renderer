#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

class VisualizeBRDFPass
{
public:
    struct PassData
    {
        RG::Resource BRDF{};
        RG::Resource ColorOut{};

        Sampler BRDFSampler{};
        
        RG::PipelineData* PipelineData{nullptr};    
    };
public:
    VisualizeBRDFPass(RG::Graph& renderGraph);
    void AddToGraph(RG::Graph& renderGraph, const Texture& brdf, RG::Resource colorIn,
        const glm::uvec2 resolution);
private:
    RG::Pass* m_Pass{nullptr};

    RG::PipelineData m_PipelineData{};
};
