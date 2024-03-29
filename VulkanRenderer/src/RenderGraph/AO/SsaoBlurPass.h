#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"

enum class SsaoBlurPassKind
{
    Horizontal, Vertical
};

class SsaoBlurPass
{
public:
    struct PassData
    {
        RenderGraph::Resource SsaoIn{};
        RenderGraph::Resource SsaoOut{};
        
        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    SsaoBlurPass(RenderGraph::Graph& renderGraph, SsaoBlurPassKind kind);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource ssao, RenderGraph::Resource colorOut);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RenderGraph::Pass* m_Pass{nullptr};
    RenderGraph::PassName m_Name;

    RenderGraph::PipelineData m_PipelineData{};
};
