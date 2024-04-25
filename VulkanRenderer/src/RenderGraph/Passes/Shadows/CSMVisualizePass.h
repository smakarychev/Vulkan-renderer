#pragma once
#include "CSMPass.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGResource.h"

class CSMVisualizePass
{
public:
    struct PassData
    {
        RG::Resource ShadowMap{};
        RG::Resource CsmUbo{};

        RG::Resource ColorOut{};

        RG::PipelineData* PipelineData{nullptr};
        u32* CascadeIndex{nullptr};
    };
public:
    CSMVisualizePass(RG::Graph& renderGraph);
    void AddToGraph(RG::Graph& renderGraph, const CSMPass::PassData& csmOutput, RG::Resource colorIn);
private:
    RG::Pass* m_Pass{nullptr};

    RG::PipelineData m_PipelineData{};
    u32 m_CascadeIndex{0};
};
