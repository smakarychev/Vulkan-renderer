#pragma once

#include <string_view>
#include <vma/vk_mem_alloc.h>

#include "Buffer.h"
#include "VulkanCommon.h"

namespace assetLib
{
    enum class TextureFormat : u32;
}

struct UploadContext;
class Buffer;
class Swapchain;
class Device;

struct ImageSubresource
{
    const Image* Image;
    // TODO: FIX ME: DIRECT VKAPI USAGE
    VkImageAspectFlags Aspect;
    u32 MipMapBase;
    u32 MipMapCount{VK_REMAINING_MIP_LEVELS};
    u32 LayerBase;
    u32 LayerCount{VK_REMAINING_ARRAY_LAYERS};
};

struct ImagePixelData
{
    const u8* Pixels;
    u32 Width;
    u32 Height;
    assetLib::TextureFormat Format;
};

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
            enum class SourceInfo {None, ImageData, Asset, PixelData};
            struct AssetInfo
            {
                enum class AssetStatus {Loaded, Reused};
                std::string AssetPath;
                AssetStatus Status;
            };
            ImageData ImageData;
            VkFormat Format;
            VkExtent2D Extent;
            VkImageUsageFlags ImageUsage;
            VkImageAspectFlags ImageAspect;
            u32 MipMapCount{1};
            VkFilter MipMapFilter;
            SourceInfo SourceInfo{SourceInfo::None};
            AssetInfo AssetInfo;
            Buffer DataBuffer;
            bool CreateView{true};
        };
    public:
        Image Build();
        Image BuildManualLifetime();
        Builder& FromAssetFile(std::string_view path);
        Builder& FromPixelData(const ImagePixelData& data);
        Builder& FromImageData(const ImageData& imageData);
        // TODO: FIX ME: DIRECT VKAPI USAGE
        Builder& SetFormat(VkFormat format);
        Builder& SetExtent(VkExtent2D extent);
        Builder& CreateMipmaps(bool enable, VkFilter filter);
        Builder& CreateView(bool enable);
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
    ImageData& GetImageData() { return m_ImageData; }
    ImageDescriptorInfo CreateDescriptorInfo(VkFilter samplerFilter) const;
    ImageDescriptorInfo CreateDescriptorInfo(VkFilter samplerFilter, VkImageLayout imageLayout) const;
    ImageDescriptorInfo CreateDescriptorInfo() const;

    ImageSubresource CreateSubresource() const;
    ImageSubresource CreateSubresource(u32 mipCount, u32 layerCount) const;
    ImageSubresource CreateSubresource(u32 mipBase, u32 mipCount, u32 layerBase, u32 layerCount) const;
    
private:
    using CreateInfo = Builder::CreateInfo;
    static Image CreateImageFromAsset(const CreateInfo& createInfo);
    static Image CreateImageFromPixelData(const CreateInfo& createInfo);
    static Image CreateImageFromBuffer(const CreateInfo& createInfo);
    static Image AllocateImage(const CreateInfo& createInfo);
    static void PrepareForTransfer(const ImageSubresource& imageSubresource);
    static void PrepareForMipmap(const ImageSubresource& imageSubresource);
    static void PrepareForShaderRead(const ImageSubresource& imageSubresource);
    static void PrepareImageGeneral(const ImageSubresource& imageSubresource,
        VkImageLayout current, VkImageLayout target,
        VkAccessFlags srcAccess, VkAccessFlags dstAccess,
        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
    static void CopyBufferToImage(const Buffer& buffer, const Image& image);
    static void CreateMipMaps(const Image& image, const CreateInfo& createInfo);

    // TODO: FIX ME: DIRECT VKAPI USAGE
    static VkSampler CreateSampler(VkFilter scaleFilter, f32 maxLod);
    
private:
    ImageData m_ImageData{};
    VmaAllocation m_Allocation{VK_NULL_HANDLE};
};

using Texture = Image;
