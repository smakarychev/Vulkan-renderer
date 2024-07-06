#pragma once
#include "RenderGraph/Passes/PBR/PbrTCForwardIBLPass.h"

class VisualizeDepthPass
{
public:
    struct PassData
    {
        RG::Resource DepthIn{};
        RG::Resource ColorOut{};

        RG::PipelineData* PipelineData{nullptr};
    };
public:
    VisualizeDepthPass(RG::Graph& renderGraph, std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource depthIn, RG::Resource colorIn,
        f32 near, f32 far, bool isOrthographic);
    Utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::PipelineData m_PipelineData{};
};
