#pragma once
#include "RenderGraph/RenderGraph.h"

namespace Passes::VisualizeBRDF
{
    struct PassData
    {
        RG::Resource BRDF{};
        RG::Resource ColorOut{};
        Sampler BRDFSampler{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, Texture brdf, RG::Resource colorIn,
        const glm::uvec2& resolution);
}
