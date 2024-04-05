#pragma once
#include "RenderGraph/RenderGraph.h"

class BlitPass
{
private:
    struct PassData
    {
        RG::Resource TextureIn;
        RG::Resource TextureOut;
    };
public:
    BlitPass(std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource textureIn, RG::Resource textureOut,
        const glm::vec3& offset, const glm::vec3& size, ImageSizeType sizeType = ImageSizeType::Relative);
private:
    RG::Pass* m_Pass{nullptr};

    RG::PassName m_Name;
};
