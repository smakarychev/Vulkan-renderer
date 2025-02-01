#pragma once
#include "RenderGraph/RGResource.h"

class HiZPassContext;

namespace Passes::HiZNV
{
    struct PassData
    {
        Sampler MinMaxSampler;
        Span<const ImageSubresourceDescription> MipmapViews;

        RG::Resource DepthIn{};
        RG::Resource HiZOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
        ImageSubresourceDescription subresource, HiZPassContext& ctx);
}

