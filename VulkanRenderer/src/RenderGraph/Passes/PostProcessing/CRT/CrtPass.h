#pragma once

#include "RenderGraph/RGResource.h"
#include "RenderGraph/RGCommon.h"

class CrtPass
{
public:
    struct SettingsUBO
    {
        f32 Curvature{0.2f};
        f32 ColorSplit{0.004f};
        f32 LinesMultiplier{1.0f};
        f32 VignettePower{0.64f};
        f32 VignetteRadius{0.025f};
    };
    struct PassData
    {
        RG::Resource ColorIn;
        RG::Resource ColorOut{};
        RG::Resource TimeUbo{};
        RG::Resource SettingsUbo{};

        RG::PipelineData* PipelineData{nullptr};
        
        SettingsUBO* Settings{nullptr};
    };
public:
    CrtPass(RG::Graph& renderGraph);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource colorIn, RG::Resource colorTarget);
private:
    RG::Pass* m_Pass{nullptr};

    RG::PipelineData m_PipelineData;
    SettingsUBO m_SettingsUBO{};
};
