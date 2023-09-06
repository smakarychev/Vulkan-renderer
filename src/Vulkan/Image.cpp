#include "Image.h"

#include "core.h"
#include "Driver.h"
#include "VulkanUtils.h"

Image Image::Builder::Build()
{
    ASSERT(!m_RequiresAllocation || !m_IsImageDataSet, "Image created from exisiting `ImageData` is immutable")
    m_CreateInfo.IsFromImageData = m_IsImageDataSet;
    Image image = Image::Create(m_CreateInfo);
    Driver::s_DeletionQueue.AddDeleter([image](){ Image::Destroy(image); });

    return image;
}

Image::Builder& Image::Builder::FromImageData(const ImageData& imageData)
{
    m_CreateInfo.ImageData = imageData;
    m_RequiresAllocation = false;
    m_IsImageDataSet = true;

    return *this;
}

Image::Builder& Image::Builder::SetDevice(const Device& device)
{
    Driver::Unpack(device, m_CreateInfo);

    return *this;
}

Image::Builder& Image::Builder::SetSwapchain(const Swapchain& swapchain)
{
    Driver::Unpack(swapchain, m_CreateInfo);

    return *this;
}

Image::Builder& Image::Builder::SetFormat(VkFormat format)
{
    m_CreateInfo.Format = format;
    m_RequiresAllocation = true;

    return *this;
}

Image::Builder& Image::Builder::SetExtent(VkExtent2D extent)
{
    m_CreateInfo.Extent = extent;
    m_RequiresAllocation = true;

    return *this;
}

Image::Builder& Image::Builder::SetUsage(VkImageUsageFlagBits imageUsage, VkImageAspectFlagBits imageAspect)
{
    m_CreateInfo.ImageUsage = imageUsage;
    m_CreateInfo.ImageAspect = imageAspect;
    m_RequiresAllocation = true;

    return *this;
}

Image Image::Create(const Builder::CreateInfo& createInfo)
{
    Image image = {};
    
    if (createInfo.IsFromImageData)
        image.m_ImageData = createInfo.ImageData;
    else
        image = AllocateImage(createInfo);

    return image;
}

void Image::Destroy(const Image& image)
{
    if (image.m_Allocation != VK_NULL_HANDLE)
    {
        vkDestroyImageView(image.m_Device, image.m_ImageData.View, nullptr);
        vmaDestroyImage(Driver::s_Allocator, image.m_ImageData.Image, image.m_Allocation);
    }
}

Image Image::AllocateImage(const CreateInfo& createInfo)
{
    Image image = {};

    image.m_ImageData.Width = createInfo.Extent.width;
    image.m_ImageData.Height = createInfo.Extent.height;
    image.m_Device = createInfo.Device;
    
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
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = (VkMemoryPropertyFlags)VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VulkanCheck(vmaCreateImage(Driver::s_Allocator, &imageCreateInfo, &allocationInfo, &image.m_ImageData.Image, &image.m_Allocation, nullptr),
        "Failed to create image");

    image.m_ImageData.View = vkUtils::createImageView(createInfo.Device, image.m_ImageData.Image, createInfo.Format, createInfo.ImageAspect, 1);

    return image;
}
