#pragma once
#include "RenderGraph/RenderGraph.h"

namespace Passes::Skybox
{
    struct ProjectionUBO
    {
        glm::mat4 ProjectionInverse{1.0f};
        glm::mat4 ViewInverse{1.0f};
    };
    struct PassData
    {
        RG::Resource Skybox{};
        RG::Resource DepthOut{};
        RG::Resource ColorOut{};
        RG::Resource Projection{};

        f32 LodBias{0.0f};
    };
    
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& skybox, RG::Resource colorOut,
        RG::Resource depthIn, const glm::uvec2& resolution, f32 lodBias);
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource skybox, RG::Resource colorOut,
        RG::Resource depthIn, const glm::uvec2& resolution, f32 lodBias);
}
