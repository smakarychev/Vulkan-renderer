#pragma once

#include <string_view>
#include <vma/vk_mem_alloc.h>

#include "VulkanCommon.h"

struct UploadContext;
class Buffer;
class Swapchain;
class Device;

class Image
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Image;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            enum class SourceInfo {None, ImageData, File};
            ImageData ImageData;
            VkFormat Format;
            VkExtent2D Extent;
            VkImageUsageFlags ImageUsage;
            VkImageAspectFlags ImageAspect;
            SourceInfo SourceInfo{SourceInfo::None};
            u8* PixelArray;
        };
    public:
        Image Build();
        Image BuildManualLifetime();
        Builder& FromFile(std::string_view path);
        Builder& FromImageData(const ImageData& imageData);
        Builder& SetFormat(VkFormat format);
        Builder& SetExtent(VkExtent2D extent);
        Builder& SetUsage(VkImageUsageFlags imageUsage, VkImageAspectFlags imageAspect);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Image Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Image& image);

    const ImageData& GetImageData() const { return m_ImageData; }
private:
    using CreateInfo = Builder::CreateInfo;
    static Image AllocateImage(const CreateInfo& createInfo);
    static void CopyDataToImage(const u8* pixels, const Image& image);

    static VkSampler CreateSampler(VkFilter scaleFilter);
    
private:
    ImageData m_ImageData{};
    VmaAllocation m_Allocation{VK_NULL_HANDLE};
};

using Texture = Image;
