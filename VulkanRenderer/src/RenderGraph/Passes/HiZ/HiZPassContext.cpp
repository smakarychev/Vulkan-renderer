#include "HiZPassContext.h"

#include "HiZBlitUtilityPass.h"
#include "utils/MathUtils.h"

HiZPassContext::HiZPassContext(const glm::uvec2& resolution, DeletionQueue& deletionQueue)
    : m_DrawResolution(resolution)
{
    u32 width = MathUtils::floorToPowerOf2(resolution.x);
    u32 height = MathUtils::floorToPowerOf2(resolution.y);

    m_HiZResolution = glm::uvec2{width, height};
    
    i8 mipmapCount = std::min(MAX_MIPMAP_COUNT, ImageUtils::mipmapCount({width, height}));

    std::vector<ImageSubresourceDescription> additionalViews(mipmapCount);
    for (i8 i = 0; i < mipmapCount; i++)
        additionalViews[i] = ImageSubresourceDescription{
            .MipmapBase = i, .Mipmaps = 1, .LayerBase = 0, .Layers = 1};

    for (u32 i = 0; i < (u32)HiZReductionMode::MaxVal; i++)
    {
        m_HiZs[i] = Device::CreateImage({
            .Description = ImageDescription{
                .Width = width,
                .Height = height,
                .Mipmaps = mipmapCount,
                .Format = Format::R32_FLOAT,
                .Usage = ImageUsage::Sampled | ImageUsage::Storage,
                .AdditionalViews = additionalViews}},
            deletionQueue);

        m_MinMaxSamplers[i] = Device::CreateSampler({
            .ReductionMode = i == (u32)HiZReductionMode::Min ? SamplerReductionMode::Min : SamplerReductionMode::Max,
            .MaxLod = MAX_MIPMAP_COUNT,
            .WithAnisotropy = false});
    }

    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        using enum BufferUsage;
        m_MinMaxDepth[i] = Device::CreateBuffer({
            .SizeBytes = sizeof(Passes::HiZBlit::MinMaxDepth),
            .Usage = Ordinary | Storage | Uniform | Readback});
    }
    
    m_MipmapViews = Device::GetAdditionalImageViews(m_HiZs[0]);
}
