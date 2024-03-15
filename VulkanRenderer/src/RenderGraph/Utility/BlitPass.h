#pragma once
#include "RenderGraph/RenderGraph.h"

class BlitPass
{
private:
    struct PassData
    {
        RenderGraph::Resource TextureIn;
        RenderGraph::Resource TextureOut;
    };
public:
    BlitPass(const std::string& name);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource textureIn, RenderGraph::Resource textureOut,
        const glm::vec3& offset, const glm::vec3& size, ImageSizeType sizeType = ImageSizeType::Relative);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    std::string m_Name;
};
