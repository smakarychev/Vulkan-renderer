#include "HiZCommon.h"

#include "RenderGraph/RenderGraph.h"
#include "Vulkan/Device.h"

namespace
{
    constexpr i8 MAX_MIPMAP_COUNT = 16;
}

namespace HiZ
{
    Sampler createSampler(ReductionMode mode)
    {
        return Device::CreateSampler({
            .ReductionMode = mode == ReductionMode::Min ? SamplerReductionMode::Min : SamplerReductionMode::Max,
            .MaxLod = MAX_MIPMAP_COUNT,
            .WithAnisotropy = false});
    }

    RG::Resource createHiz(RG::Graph& renderGraph, const glm::uvec2& depthResolution)
    {
        const u32 width = Math::floorToPowerOf2(depthResolution.x);
        const u32 height = Math::floorToPowerOf2(depthResolution.y);
        const i8 mipmapCount = std::min(MAX_MIPMAP_COUNT, ImageUtils::mipmapCount({width, height}));

        std::vector<ImageSubresourceDescription> additionalViews(mipmapCount);
        for (i8 i = 0; i < mipmapCount; i++)
            additionalViews[i] = ImageSubresourceDescription{
                .MipmapBase = i, .Mipmaps = 1, .LayerBase = 0, .Layers = 1};

        return renderGraph.CreateResource("HiZ"_hsv, RG::GraphTextureDescription{
            .Width = width,
            .Height = height,
            .Mipmaps = mipmapCount,
            .Format = Format::R32_FLOAT,
            .AdditionalViews = additionalViews});
    }

    RG::Resource createMinMaxBuffer(RG::Graph& renderGraph)
    {
        return renderGraph.CreateResource("MinMax"_hsv, RG::GraphBufferDescription{
            .SizeBytes = sizeof(MinMaxDepth)});
    }
}
