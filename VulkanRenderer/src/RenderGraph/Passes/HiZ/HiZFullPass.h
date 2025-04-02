#pragma once
#include "RenderGraph/RGResource.h"

class HiZPassContext;

namespace Passes::HiZFull
{
    struct PassData
    {
        RG::Resource MinMaxDepth{};
        Sampler MinSampler;
        Sampler MaxSampler;
        Span<const ImageSubresourceDescription> MipmapViews;
        RG::Resource DepthMin{};
        RG::Resource DepthMax{};
        RG::Resource HiZMinOut{};
        RG::Resource HiZMaxOut{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource depth,
        ImageSubresourceDescription subresource, HiZPassContext& ctx);
}
