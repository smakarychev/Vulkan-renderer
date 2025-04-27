#pragma once
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
        RG::Resource csmTexture, RG::Resource csmInfo, RG::Resource colorIn);
}
