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

    RG::Resource createHiz(RG::Graph& renderGraph, const glm::uvec2& depthResolution, ReductionMode mode)
    {
        const f32 width = (f32)Math::floorToPowerOf2(depthResolution.x);
        const f32 height = (f32)Math::floorToPowerOf2(depthResolution.y);
        const i8 mipmapCount = std::min(MAX_MIPMAP_COUNT, Images::mipmapCount({width, height}));

        return renderGraph.Create("HiZ"_hsv, RG::RGImageDescription{
            .Width = width,
            .Height = height,
            .Mipmaps = mipmapCount,
            .Format = mode == ReductionMode::MinMax ? Format::RG16_FLOAT : Format::R16_FLOAT});
    }

    RG::Resource createMinMaxBufferResource(RG::Graph& renderGraph)
    {
        return renderGraph.Create("MinMax"_hsv, RG::RGBufferDescription{
            .SizeBytes = sizeof(MinMaxDepth)});
    }
}
