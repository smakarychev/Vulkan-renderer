#pragma once

#include <vma/vk_mem_alloc.h>

#include "VulkanCommon.h"

class Swapchain;
class Device;

class Image
{
public:
    class Builder
    {
        friend class Image;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            ImageData ImageData;
            VkFormat Format;
            VkExtent2D Extent;
            VkImageUsageFlagBits ImageUsage;
            VkImageAspectFlagBits ImageAspect;
            bool IsFromImageData;
        };
    public:
        Image Build();
        Builder& FromImageData(const ImageData& imageData);
        Builder& SetFormat(VkFormat format);
        Builder& SetExtent(VkExtent2D extent);
        Builder& SetUsage(VkImageUsageFlagBits imageUsage, VkImageAspectFlagBits imageAspect);
    private:
        CreateInfo m_CreateInfo;
        bool m_RequiresAllocation{true};
        bool m_IsImageDataSet{false};
    };
public:
    static Image Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Image& image);

    const ImageData& GetImageData() const { return m_ImageData; }
private:
    using CreateInfo = Builder::CreateInfo;
    static Image AllocateImage(const CreateInfo& createInfo);
private:
    ImageData m_ImageData{};
    VmaAllocation m_Allocation{VK_NULL_HANDLE};
};
