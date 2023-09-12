#include "Image.h"

#include "core.h"
#include "Driver.h"
#include "VulkanUtils.h"

#include "Buffer.h"
#include "RenderCommand.h"
#include "AssetLib.h"
#include "TextureAsset.h"

Image Image::Builder::Build()
{
    Image image = Image::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([image](){ Image::Destroy(image); });

    return image;
}

Image Image::Builder::BuildManualLifetime()
{
    return Image::Create(m_CreateInfo);
}

Image::Builder& Image::Builder::FormAssetFile(std::string_view path)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, "`Asset` in `ImageData` options are incompatible")
    
    m_CreateInfo.SourceInfo = CreateInfo::SourceInfo::Asset;

    assetLib::File textureFile;
    assetLib::loadBinaryFile(path, textureFile);
    assetLib::TextureInfo textureInfo = assetLib::readTextureInfo(textureFile);
    ASSERT(textureInfo.Format == assetLib::TextureFormat::SRGBA8, "Unsopported image format")
    
    m_CreateInfo.AssetBuffer = Buffer::Builder().
        SetKind(BufferKind::Source).
        SetSizeBytes(textureInfo.SizeBytes).
        SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT).
        BuildManualLifetime();
    
    void* dst = m_CreateInfo.AssetBuffer.Map();
    assetLib::unpackTexture(textureInfo, textureFile.Blob.data(), textureFile.Blob.size(), (u8*)dst);
    m_CreateInfo.AssetBuffer.Unmap();

    m_CreateInfo.Format = VK_FORMAT_R8G8B8A8_SRGB;
    m_CreateInfo.Extent = {textureInfo.Dimensions.Width, textureInfo.Dimensions.Height};
    
    return *this;
}

Image::Builder& Image::Builder::FromImageData(const ImageData& imageData)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset, "`Asset` in `ImageData` options are incompatible")
    m_CreateInfo.ImageData = imageData;
    m_CreateInfo.SourceInfo = CreateInfo::SourceInfo::ImageData;

    return *this;
}

Image::Builder& Image::Builder::SetFormat(VkFormat format)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, " Images created using `ImageData` option are immutable")
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset || format == VK_FORMAT_R8G8B8A8_SRGB,
        "Cannot use custom format when loading from file")
    m_CreateInfo.Format = format;
    
    return *this;
}

Image::Builder& Image::Builder::SetExtent(VkExtent2D extent)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, " Images created using `ImageData` option are immutable")
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset,
        "Cannot set extent when loading from file")
    m_CreateInfo.Extent = extent;

    return *this;
}

Image::Builder& Image::Builder::SetUsage(VkImageUsageFlags imageUsage, VkImageAspectFlags imageAspect)
{
    m_CreateInfo.ImageUsage = imageUsage;
    m_CreateInfo.ImageAspect = imageAspect;

    return *this;
}

Image Image::Create(const Builder::CreateInfo& createInfo)
{
    Image image = {};

    switch (createInfo.SourceInfo)
    {
    case CreateInfo::SourceInfo::None:
        image = AllocateImage(createInfo);
        image.m_ImageData.View = vkUtils::createImageView(Driver::DeviceHandle(), image.m_ImageData.Image, createInfo.Format, createInfo.ImageAspect, 1);
        break;
    case CreateInfo::SourceInfo::ImageData:
        image.m_ImageData = createInfo.ImageData;
        break;
    case CreateInfo::SourceInfo::Asset:
        image = AllocateImage(createInfo);
        CopyBufferToImage(createInfo.AssetBuffer, image);
        image.m_ImageData.View = vkUtils::createImageView(Driver::DeviceHandle(), image.m_ImageData.Image, createInfo.Format, createInfo.ImageAspect, 1);
        break;
    default:
        ASSERT(false, "Unrecognized image source info")
        std::unreachable();
    }

    return image;
}

void Image::Destroy(const Image& image)
{
    if (image.m_Allocation != VK_NULL_HANDLE)
    {
        vkDestroyImageView(Driver::DeviceHandle(), image.m_ImageData.View, nullptr);
        vmaDestroyImage(Driver::Allocator(), image.m_ImageData.Image, image.m_Allocation);
    }
}

Image Image::AllocateImage(const CreateInfo& createInfo)
{
    Image image = {};

    image.m_ImageData.Width = createInfo.Extent.width;
    image.m_ImageData.Height = createInfo.Extent.height;
    
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.format = createInfo.Format;
    imageCreateInfo.usage = createInfo.ImageUsage;
    imageCreateInfo.extent = {.width = createInfo.Extent.width, .height = createInfo.Extent.height, .depth = 1};
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VulkanCheck(vmaCreateImage(Driver::Allocator(), &imageCreateInfo, &allocationInfo, &image.m_ImageData.Image, &image.m_Allocation, nullptr),
        "Failed to create image");

    return image;
}

void Image::CopyBufferToImage(const Buffer& buffer, const Image& image)
{
    Driver::ImmediateUpload([&buffer, &image](const CommandBuffer& cmd)
    {
        RenderCommand::CopyBufferToImage(cmd, buffer, image);
    });
    
    Buffer::Destroy(buffer);
}

VkSampler Image::CreateSampler(VkFilter scaleFilter)
{
    VkSampler sampler;
    
    VkSamplerCreateInfo samplerCreateInfo = {};
    
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = scaleFilter;
    samplerCreateInfo.minFilter = scaleFilter;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    vkCreateSampler(Driver::DeviceHandle(), &samplerCreateInfo, nullptr, &sampler);
    Driver::DeletionQueue().AddDeleter([sampler](){ vkDestroySampler(Driver::DeviceHandle(), sampler, nullptr); });

    return sampler;
}
