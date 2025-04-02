#pragma once
#include "CSMPass.h"
#include "RenderGraph/RGResource.h"

namespace Passes::VisualizeCSM
{
    struct PassData
    {
        RG::Resource ShadowMap{};
        RG::Resource CSM{};

        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        const CSM::PassData& csmOutput, RG::Resource colorIn);
}
