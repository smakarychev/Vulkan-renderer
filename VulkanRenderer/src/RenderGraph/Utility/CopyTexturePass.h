#pragma once
#include "RenderGraph/RenderGraph.h"

class CopyTexturePass
{
private:
    struct PassData
    {
        RenderGraph::Resource TextureIn;
        RenderGraph::Resource TextureOut;
    };
public:
    CopyTexturePass(std::string_view name);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource textureIn, RenderGraph::Resource textureOut,
        const glm::vec3& offset, const glm::vec3& size, ImageSizeType sizeType = ImageSizeType::Relative);
public:
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::PassName m_Name;
};
