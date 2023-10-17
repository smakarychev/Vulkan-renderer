#pragma once

#include <string_view>
#include <vma/vk_mem_alloc.h>

#include "Buffer.h"
#include "VulkanCommon.h"

struct UploadContext;
class Buffer;
class Swapchain;
class Device;

using ImageDescriptorInfo = VkDescriptorImageInfo;
using TextureDescriptorInfo = ImageDescriptorInfo;

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
            enum class SourceInfo {None, ImageData, Asset};
            ImageData ImageData;
            VkFormat Format;
            VkExtent2D Extent;
            VkImageUsageFlags ImageUsage;
            VkImageAspectFlags ImageAspect;
            u32 MipMapCount{1};
            SourceInfo SourceInfo{SourceInfo::None};
            Buffer AssetBuffer;
        };
    public:
        Image Build();
        Image BuildManualLifetime();
        Builder& FromAssetFile(std::string_view path);
        Builder& FromImageData(const ImageData& imageData);
        Builder& SetFormat(VkFormat format);
        Builder& SetExtent(VkExtent2D extent);
        Builder& CreateMipmaps(bool enable);
        Builder& SetUsage(VkImageUsageFlags imageUsage, VkImageAspectFlags imageAspect);
    private:
        void PreBuild();
    private:
        CreateInfo m_CreateInfo;
        bool m_CreateMipmaps{false};
    };
public:
    static Image Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Image& image);

    const ImageData& GetImageData() const { return m_ImageData; }
    ImageDescriptorInfo CreateDescriptorInfo(VkFilter samplerFilter) const;
    ImageDescriptorInfo CreateDescriptorInfo() const;
private:
    using CreateInfo = Builder::CreateInfo;
    static Image AllocateImage(const CreateInfo& createInfo);
    static void PrepareForTransfer(const Image& image, const ImageSubresource& imageSubresource);
    static void PrepareForMipmap(const Image& image, const ImageSubresource& imageSubresource);
    static void PrepareForShaderRead(const Image& image, const ImageSubresource& imageSubresource);
    static void PrepareImageGeneral(const Image& image, const ImageSubresource& imageSubresource,
        VkImageLayout current, VkImageLayout target,
        VkAccessFlags srcAccess, VkAccessFlags dstAccess,
        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
    static void CopyBufferToImage(const Buffer& buffer, const Image& image, VkImageAspectFlags imageAspect);
    static void CreateMipMaps(const Image& image, const CreateInfo& createInfo);

    static VkSampler CreateSampler(VkFilter scaleFilter, f32 maxLod);
    
private:
    ImageData m_ImageData{};
    VmaAllocation m_Allocation{VK_NULL_HANDLE};
};

using Texture = Image;
