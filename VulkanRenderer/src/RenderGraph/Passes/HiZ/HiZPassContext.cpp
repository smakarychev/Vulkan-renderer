#include "HiZPassContext.h"

#include "HiZBlitUtilityPass.h"
#include "HiZFullPass.h"
#include "utils/MathUtils.h"

HiZPassContext::HiZPassContext(const glm::uvec2& resolution, DeletionQueue& deletionQueue)
    : m_DrawResolution(resolution)
{
    u32 width = MathUtils::floorToPowerOf2(resolution.x);
    u32 height = MathUtils::floorToPowerOf2(resolution.y);

    m_HiZResolution = glm::uvec2{width, height};
    
    i8 mipmapCount = std::min(MAX_MIPMAP_COUNT, Image::CalculateMipmapCount({width, height}));

    std::vector<ImageSubresourceDescription> additionalViews(mipmapCount);
    for (i8 i = 0; i < mipmapCount; i++)
        additionalViews[i] = ImageSubresourceDescription{
            .MipmapBase = (u8)i, .Mipmaps = 1, .LayerBase = 0, .Layers = 1};

    for (u32 i = 0; i < (u32)HiZReductionMode::MaxVal; i++)
    {
        m_HiZs[i] = Texture::Builder({
                .Width = width,
                .Height = height,
                .Mipmaps = mipmapCount,
                .Format = Format::R32_FLOAT,
                .Usage = ImageUsage::Sampled | ImageUsage::Storage,
                .AdditionalViews = additionalViews})
            .Build(deletionQueue);

        m_MinMaxSamplers[i] = Sampler::Builder()
            .Filters(ImageFilter::Linear, ImageFilter::Linear)
            .MaxLod(MAX_MIPMAP_COUNT)
            .WithAnisotropy(false)
            .ReductionMode(SamplerReductionMode::Min)
            .Build();
    }

    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        using enum BufferUsage;
        m_MinMaxDepth[i] = Device::CreateBuffer({
            .SizeBytes = sizeof(Passes::HiZBlit::MinMaxDepth),
            .Usage = Ordinary | Storage | Uniform | Readback});
        Device::DeletionQueue().Enqueue(m_MinMaxDepth[i]);
    }
    
    m_MipmapViewHandles = m_HiZs[0].GetAdditionalViewHandles();
}
