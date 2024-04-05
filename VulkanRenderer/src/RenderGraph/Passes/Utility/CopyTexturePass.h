#pragma once
#include "RenderGraph/RenderGraph.h"

class CopyTexturePass
{
public:
    struct PassData
    {
        RG::Resource TextureIn;
        RG::Resource TextureOut;
    };
public:
    CopyTexturePass(std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource textureIn, RG::Resource textureOut,
        const glm::vec3& offset, const glm::vec3& size, ImageSizeType sizeType = ImageSizeType::Relative);
public:
private:
    RG::Pass* m_Pass{nullptr};

    RG::PassName m_Name;
};
