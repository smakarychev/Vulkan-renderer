#pragma once
#include "RenderGraph/RenderGraph.h"

namespace Passes::CopyTexture
{
    struct PassData
    {
        RG::Resource TextureIn;
        RG::Resource TextureOut;
    };
    
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource textureIn, RG::Resource textureOut,
       const glm::vec3& offset, const glm::vec3& size, ImageSizeType sizeType = ImageSizeType::Relative);
}
