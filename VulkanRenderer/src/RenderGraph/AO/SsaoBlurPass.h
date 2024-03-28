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
private:
    RenderGraph::Pass* m_Pass{nullptr};
    RenderGraph::PassName m_Name;

    RenderGraph::PipelineData m_PipelineData{};
};
