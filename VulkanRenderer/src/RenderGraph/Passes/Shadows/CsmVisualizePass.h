#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::VisualizeCsm
{
    struct PassData
    {
        RG::Resource ShadowMap{};

        RG::Resource ColorOut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource csmTexture);
}
