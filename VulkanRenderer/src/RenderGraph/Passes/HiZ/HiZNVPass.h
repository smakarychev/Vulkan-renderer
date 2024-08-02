#pragma once
#include "HiZPass.h"
#include "RenderGraph/RGResource.h"

namespace Passes::HiZNV
{
    struct PassData
    {
        Sampler MinMaxSampler;
        std::vector<ImageViewHandle> MipmapViewHandles;

        RG::Resource DepthIn{};
        RG::Resource HiZOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
        ImageSubresourceDescription::Packed subresource, HiZPassContext& ctx);
}

