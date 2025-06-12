#include "HiZCommon.h"

#include "RenderGraph/RGGraph.h"
#include "Rendering/Image/ImageUtility.h"
#include "Vulkan/Device.h"

namespace
{
    constexpr i8 MAX_MIPMAP_COUNT = 16;
}

namespace HiZ
{
    glm::uvec2 calculateHizResolution(const glm::uvec2& depthResolution)
    {
        return Images::floorResolutionToPowerOfTwo(depthResolution);
    }

    Sampler createSampler(ReductionMode mode)
    {
        return Device::CreateSampler({
            .ReductionMode = mode == ReductionMode::Min ? SamplerReductionMode::Min : SamplerReductionMode::Max,
            .MaxLod = MAX_MIPMAP_COUNT,
            .WithAnisotropy = false});
    }

    RG::Resource createHiz(RG::Graph& renderGraph, const glm::uvec2& depthResolution)
    {
        const f32 width = (f32)Math::floorToPowerOf2(depthResolution.x);
        const f32 height = (f32)Math::floorToPowerOf2(depthResolution.y);
        const i8 mipmapCount = std::min(MAX_MIPMAP_COUNT, Images::mipmapCount({width, height}));

        std::vector<ImageSubresourceDescription> additionalViews(mipmapCount);
        for (i8 i = 0; i < mipmapCount; i++)
            additionalViews[i] = ImageSubresourceDescription{
                .MipmapBase = i, .Mipmaps = 1, .LayerBase = 0, .Layers = 1};

        return renderGraph.Create("HiZ"_hsv, RG::RGImageDescription{
            .Width = width,
            .Height = height,
            .Mipmaps = mipmapCount,
            .Format = Format::R32_FLOAT});
    }

    RG::Resource createMinMaxBufferResource(RG::Graph& renderGraph)
    {
        return renderGraph.Create("MinMax"_hsv, RG::RGBufferDescription{
            .SizeBytes = sizeof(MinMaxDepth)});
    }
}
