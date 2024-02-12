#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>
#include <vma/vk_mem_alloc.h>

#include "Buffer.h"
#include "ImageTraits.h"
#include "SynchronizationTraits.h"
#include "VulkanCommon.h"

class ImageViewList;
struct LayoutTransitionInfo;

namespace assetLib
{
    enum class TextureFormat : u32;
}

struct UploadContext;
class Buffer;
class Swapchain;
class Device;

class Sampler
{
    FRIEND_INTERNAL
    friend class Image;
    friend class SamplerCache;
public:
    static constexpr f32 LOD_MAX = 1000.0f;
    
    class Builder
    {
        friend class Sampler;
        friend class SamplerCache;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            VkFilter MinificationFilter;
            VkFilter MagnificationFilter;
            VkSamplerMipmapMode MipmapFilter;
            VkSamplerAddressMode AddressMode{VK_SAMPLER_ADDRESS_MODE_REPEAT};
            std::optional<VkSamplerReductionMode> ReductionMode;
            f32 MaxLod{LOD_MAX};
            bool WithAnisotropy{true};
        };
    public:
        Sampler Build();
        Builder& Filters(ImageFilter minification, ImageFilter magnification);
        Builder& WrapMode(SamplerWrapMode mode);
        Builder& ReductionMode(SamplerReductionMode reductionMode);
        Builder& MaxLod(f32 lod);
        Builder& WithAnisotropy(bool enabled);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static void Destroy(const Sampler& sampler);
private:
    VkSampler m_Sampler{VK_NULL_HANDLE};
};

class ImageViewHandle
{
    static constexpr u32 NON_INDEX = std::numeric_limits<u32>::max();
    friend class ImageViewList;
    friend class Builder;
private:
    u32 m_Index{NON_INDEX};
};

struct ImageDescription
{
    static constexpr u32 ALL_MIPMAPS = ~0u;
    static constexpr u32 ALL_LAYERS = ~0u;
    
    u32 Width{0};
    u32 Height{0};
    u32 Layers{1};
    u16 Mipmaps{1};
    u16 Views{1};
    ImageFormat Format{ImageFormat::Undefined};
    ImageKind Kind{ImageKind::Image2d};
    ImageUsage Usage{ImageUsage::None};
    ImageFilter MipmapFilter{ImageFilter::Linear};
};

struct ImageSubresource
{
    const Image* Image;
    u32 MipmapBase;
    u32 Mipmaps{ImageDescription::ALL_MIPMAPS};
    u32 LayerBase;
    u32 Layers{ImageDescription::ALL_LAYERS};
};

struct ImageBlitInfo
{
    const Image* Image;
    u32 MipmapBase;
    u32 LayerBase;
    u32 Layers{ImageDescription::ALL_LAYERS};
    glm::uvec3 Bottom;
    glm::uvec3 Top;
};

struct ImageBindingInfo
{
    const Image* Image;
    Sampler Sampler;
    ImageLayout Layout;
    
    const ImageViewList* ViewList{nullptr};
    ImageViewHandle ViewHandle{};
};

class Image
{
    FRIEND_INTERNAL
    friend class ImageViewList;
public:
    class Builder
    {
        friend class Image;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            enum class SourceInfo {None, Asset, Pixels};
            struct AssetInfo
            {
                enum class AssetStatus {Loaded, Reused};
                std::string AssetPath;
                AssetStatus Status;
            };
            VkFormat Format;
            VkImageUsageFlags ImageUsage;
            VkImageAspectFlags ImageAspect;
            VkFilter MipmapFilter;
            SourceInfo SourceInfo{SourceInfo::None};
            AssetInfo AssetInfo;
            Buffer DataBuffer;
            ImageDescription Description{};
        };
    public:
        Builder() = default;
        Builder(const ImageDescription& description);
        Image Build();
        Image BuildManualLifetime();
        Builder& FromAssetFile(std::string_view path);
        template <typename T>
        Builder& FromPixels(const std::vector<T>& pixels)
        {
            return FromPixels(pixels.data(), pixels.size() * sizeof(T));
        }
        Builder& SetFormat(ImageFormat format);
        Builder& SetExtent(const glm::uvec2& extent);
        Builder& SetExtent(const glm::uvec3& extent);
        Builder& CreateMipmaps(bool enable, ImageFilter filter);
        Builder& SetUsage(ImageUsage usage);
    private:
        void PreBuild();
        Builder& FromPixels(const void* pixels, u64 sizeBytes);
    private:
        CreateInfo m_CreateInfo;
        bool m_CreateMipmaps{false};
    };
public:
    static Image Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Image& image);

    const ImageDescription& GetDescription() const { return m_Description; }
    
    ImageSubresource CreateSubresource() const;
    ImageSubresource CreateSubresource(u32 mipCount, u32 layerCount) const;
    ImageSubresource CreateSubresource(u32 mipBase, u32 mipCount, u32 layerBase, u32 layerCount) const;

    ImageBlitInfo CreateImageBlitInfo() const;
    ImageBlitInfo CreateImageBlitInfo(u32 mipBase, u32 layerBase, u32 layerCount) const;
    ImageBlitInfo CreateImageBlitInfo(const glm::uvec3& bottom, const glm::uvec3& top,
        u32 mipBase, u32 layerBase, u32 layerCount) const;

    ImageBindingInfo CreateBindingInfo(ImageFilter filter, ImageLayout layout) const;
    ImageBindingInfo CreateBindingInfo(Sampler sampler, ImageLayout layout) const;
    ImageBindingInfo CreateBindingInfo(ImageFilter filter, ImageLayout layout,
        const ImageViewList& views, ImageViewHandle handle) const;
    ImageBindingInfo CreateBindingInfo(Sampler sampler, ImageLayout layout,
        const ImageViewList& views, ImageViewHandle handle) const;
private:
    using CreateInfo = Builder::CreateInfo;
    static Image CreateImageFromAsset(const CreateInfo& createInfo);
    static Image CreateImageFromPixelData(const CreateInfo& createInfo);
    static Image CreateImageFromBuffer(const CreateInfo& createInfo);
    static Image AllocateImage(const CreateInfo& createInfo);
    static void PrepareForMipmapDestination(const ImageSubresource& imageSubresource);
    static void PrepareForMipmapSource(const ImageSubresource& imageSubresource);
    static void PrepareForShaderRead(const ImageSubresource& imageSubresource);
    static void PrepareImageGeneral(const ImageSubresource& imageSubresource,
        ImageLayout current, ImageLayout target,
        PipelineAccess srcAccess, PipelineAccess dstAccess,
        PipelineStage srcStage, PipelineStage dstStage);
    static void CopyBufferToImage(const Buffer& buffer, const Image& image);
    static void CreateMipmaps(const Image& image, const CreateInfo& createInfo);

    static VkImageView CreateVulkanImageView(const ImageSubresource& imageSubresource);
    static VkImageView CreateVulkanImageView(const ImageSubresource& imageSubresource, VkFormat format);
    static void FillVulkanLayoutTransitionBarrier(const LayoutTransitionInfo& layoutTransitionInfo,
        VkImageMemoryBarrier2& barrier);
    static std::pair<VkBlitImageInfo2, VkImageBlit2> CreateVulkanBlitInfo(
        const ImageBlitInfo& source, const ImageBlitInfo& destination, ImageFilter filter);
    static VkBufferImageCopy2 CreateVulkanImageCopyInfo(const ImageSubresource& subresource);
    static VkDescriptorImageInfo CreateVulkanImageDescriptor(const ImageBindingInfo& imageBindingInfo);
    static VkRenderingAttachmentInfo CreateVulkanRenderingAttachment(const Image& image, ImageLayout layout);
    static VkPipelineRenderingCreateInfo CreateVulkanRenderingInfo(
        const RenderingDetails& renderingDetails, std::vector<VkFormat>& colorFormats);
    
private:
    VkImage m_Image{VK_NULL_HANDLE};
    VkImageView m_View{VK_NULL_HANDLE};
    ImageDescription m_Description{};
    VmaAllocation m_Allocation{VK_NULL_HANDLE};
};

class ImageViewList
{
    friend class Image;
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class ImageViewList;
        struct CreateInfo
        {
            const Image* Image;
            std::vector<VkImageView> ImageViews;
        };
    public:
        ImageViewList Build();
        ImageViewList BuildManualLifetime();
        Builder& ForImage(const Image& image);
        Builder& Add(const ImageSubresource& subresource, ImageViewHandle& handle);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static ImageViewList Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const ImageViewList& imageViews);
private:
    VkImageView operator[](ImageViewHandle handle) const
    {
        return m_Views[handle.m_Index];
    }
private:
    const Image* m_Image{nullptr};
    std::vector<VkImageView> m_Views;
};

using Texture = Image;

class SamplerCache
{
public:
    static Sampler CreateSampler(const Sampler::Builder::CreateInfo& createInfo);
private:
    struct CacheKey
    {
        Sampler::Builder::CreateInfo CreateInfo;
        bool operator==(const CacheKey& other) const;
    };
    struct SamplerKeyHash
    {
        u64 operator()(const CacheKey& cacheKey) const;
    };

    static std::unordered_map<CacheKey, Sampler, SamplerKeyHash> s_SamplerCache;
};