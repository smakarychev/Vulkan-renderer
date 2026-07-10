#pragma once
#include "RenderGraph/RGGraph.h"

namespace Passes::Blit
{
struct PassData
{
    RG::ImageResource TextureIn;
    RG::ImageResource TextureOut;
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, RG::ImageResource textureIn, RG::ImageResource textureOut,
    const glm::vec3& offset, f32 relativeSize, ImageFilter filter);
}
