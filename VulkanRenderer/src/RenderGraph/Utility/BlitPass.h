#pragma once
#include "RenderGraph/RenderGraph.h"

class BlitPass
{
public:
    struct PassData
    {
        RenderGraph::Resource TextureIn;
        RenderGraph::Resource TextureOut;
    };
public:
    BlitPass(RenderGraph::Graph& renderGraph, RenderGraph::Resource textureIn, RenderGraph::Resource colorTarget);
private:
    RenderGraph::Pass* m_Pass{nullptr};
};
