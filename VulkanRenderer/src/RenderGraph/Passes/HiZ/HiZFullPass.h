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
        std::vector<ImageViewHandle> MipmapViewHandles;
        RG::Resource DepthMin{};
        RG::Resource DepthMax{};
        RG::Resource HiZMinOut{};
        RG::Resource HiZMaxOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
        ImageSubresourceDescription subresource, HiZPassContext& ctx);
}
