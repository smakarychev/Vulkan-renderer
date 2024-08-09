#pragma once
#include "RenderGraph/RGResource.h"

class HiZPassContext;

namespace Passes::DepthReduction
{
    struct MinMaxDepth
    {
        f32 Min{1.0f};
        f32 Max{0.0f};
    };
    struct PassData
    {
        RG::Resource MinMaxDepth;
        Sampler MinSampler;
        Sampler MaxSampler;
        std::vector<ImageViewHandle> MipmapViewHandles;
        RG::Resource DepthMin{};
        RG::Resource DepthMax{};
        RG::Resource HiZMinOut{};
        RG::Resource HiZMaxOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
        ImageSubresourceDescription::Packed subresource, HiZPassContext& ctx);
}
