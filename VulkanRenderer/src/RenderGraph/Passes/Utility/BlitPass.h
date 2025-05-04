#pragma once
#include "RenderGraph/RenderGraph.h"

namespace Passes::Blit
{
    struct PassData
    {
        RG::Resource TextureIn;
        RG::Resource TextureOut;
    };

    PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn, RG::Resource textureOut,
        const glm::vec3& offset, f32 relativeSize, ImageFilter filter);
}
