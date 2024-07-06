#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

enum class SsaoBlurPassKind
{
    Horizontal, Vertical
};

class SsaoBlurPass
{
public:
    struct PassData
    {
        RG::Resource SsaoIn{};
        RG::Resource SsaoOut{};
        
        RG::PipelineData* PipelineData{nullptr};
    };
public:
    SsaoBlurPass(RG::Graph& renderGraph, SsaoBlurPassKind kind);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource ssao, RG::Resource colorOut);
    Utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::PipelineData m_PipelineData{};
};
