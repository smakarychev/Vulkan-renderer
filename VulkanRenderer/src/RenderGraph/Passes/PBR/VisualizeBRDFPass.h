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
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, Texture brdf, RG::Resource colorIn,
        const glm::uvec2& resolution);
}
