#include "Image.h"

#include "core.h"
#include "Driver.h"
#include "VulkanUtils.h"

#include <stb_image.h>

#include "Buffer.h"
#include "RenderCommand.h"

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

Image::Builder& Image::Builder::FromFile(std::string_view path)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, "`File` in `ImageData` options are incompatible")
    stbi_set_flip_vertically_on_load(true);
    
    m_CreateInfo.SourceInfo = CreateInfo::SourceInfo::File;

    i32 width, height, channels;
    m_CreateInfo.PixelArray = stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);
    m_CreateInfo.Format = VK_FORMAT_R8G8B8A8_SRGB;
    m_CreateInfo.Extent = {(u32)width, (u32)height};
    
    return *this;
}

Image::Builder& Image::Builder::FromImageData(const ImageData& imageData)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::File, "`File` in `ImageData` options are incompatible")
    m_CreateInfo.ImageData = imageData;
    m_CreateInfo.SourceInfo = CreateInfo::SourceInfo::ImageData;

    return *this;
}

Image::Builder& Image::Builder::SetFormat(VkFormat format)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, " Images created using `ImageData` option are immutable")
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::File || format == VK_FORMAT_R8G8B8A8_SRGB,
        "Cannot use custom format when loading from file")
    m_CreateInfo.Format = format;
    
    return *this;
}

Image::Builder& Image::Builder::SetExtent(VkExtent2D extent)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, " Images created using `ImageData` option are immutable")
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::File,
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
    case CreateInfo::SourceInfo::File:
        image = AllocateImage(createInfo);
        CopyDataToImage(createInfo.PixelArray, image);
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

void Image::CopyDataToImage(const u8* pixels, const Image& image)
{
    VkDeviceSize sizeBytes = 4llu * image.m_ImageData.Width * image.m_ImageData.Height;

    Buffer stageBuffer = Buffer::Builder().
        SetKind(BufferKind::Source).
        SetSizeBytes(sizeBytes).
        SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT).
        BuildManualLifetime();

    stageBuffer.SetData(pixels, sizeBytes);

    Driver::ImmediateUpload([&stageBuffer,&image](const CommandBuffer& cmd)
    {
        RenderCommand::CopyBufferToImage(cmd, stageBuffer, image);
    });
    
    Buffer::Destroy(stageBuffer);
    stbi_image_free((void*)pixels);
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
