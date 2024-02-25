#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>

#include "Buffer.h"
#include "ImageTraits.h"
#include "FormatTraits.h"
#include "SynchronizationTraits.h"

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
            ImageFilter MinificationFilter{ImageFilter::Linear};
            ImageFilter MagnificationFilter{ImageFilter::Linear};
            SamplerWrapMode AddressMode{SamplerWrapMode::Repeat};
            std::optional<SamplerReductionMode> ReductionMode;
            f32 MaxLod{LOD_MAX};
            bool WithAnisotropy{true};
        };
    public:
        Sampler Build();
        Builder& Filters(ImageFilter minification, ImageFilter magnification);
        Builder& WrapMode(SamplerWrapMode mode);
        Builder& ReductionMode(SamplerReductionMode mode);
        Builder& MaxLod(f32 lod);
        Builder& WithAnisotropy(bool enabled);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Sampler Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Sampler& sampler);
private:
    ResourceHandle<Sampler> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandle<Sampler> m_ResourceHandle;
};

class ImageViewHandle
{
    static constexpr u32 NON_INDEX = std::numeric_limits<u32>::max();
    friend class ImageViewList;
    friend class Image;
    FRIEND_INTERNAL
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
    Format Format{Format::Undefined};
    ImageKind Kind{ImageKind::Image2d};
    ImageUsage Usage{ImageUsage::None};
    ImageFilter MipmapFilter{ImageFilter::Linear};
};
using TextureDescription = ImageDescription;

struct ImageSubresourceDescription
{
    u32 MipmapBase;
    u32 Mipmaps{ImageDescription::ALL_MIPMAPS};
    u32 LayerBase;
    u32 Layers{ImageDescription::ALL_LAYERS};
};

struct ImageSubresource
{
    const Image* Image;
    ImageSubresourceDescription Description;
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
    
    ImageViewHandle ViewHandle{};
};

class Image
{
    friend class ImageViewList;
    FRIEND_INTERNAL
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
            SourceInfo SourceInfo{SourceInfo::None};
            AssetInfo AssetInfo;
            Buffer DataBuffer;
            ImageDescription Description{};
            bool CreateMipmaps{false};
            bool ViewCountFromDescription{false};
            std::vector<ImageSubresourceDescription> AdditionalViews;
        };
    public:
        Builder() = default;
        Builder(const ImageDescription& description);
        Image Build();
        Image Build(DeletionQueue& deletionQueue);
        Image BuildManualLifetime();
        Builder& FromAssetFile(std::string_view path);
        template <typename T>
        Builder& FromPixels(const std::vector<T>& pixels)
        {
            return FromPixels(pixels.data(), pixels.size() * sizeof(T));
        }
        Builder& SetFormat(Format format);
        Builder& SetExtent(const glm::uvec2& extent);
        Builder& SetExtent(const glm::uvec3& extent);
        Builder& SetKind(ImageKind kind);
        Builder& CreateMipmaps(bool enable, ImageFilter filter);
        Builder& SetUsage(ImageUsage usage);
        Builder& AddView(const ImageSubresourceDescription& subresource, ImageViewHandle& viewHandle);
    private:
        void PreBuild();
        Builder& FromPixels(const void* pixels, u64 sizeBytes);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Image Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Image& image);

    const ImageDescription& GetDescription() const { return m_Description; }
    
    ImageSubresource CreateSubresource() const;
    ImageSubresource CreateSubresource(u32 mipCount, u32 layerCount) const;
    ImageSubresource CreateSubresource(u32 mipBase, u32 mipCount, u32 layerBase, u32 layerCount) const;
    ImageSubresource CreateSubresource(const ImageSubresourceDescription& description) const;

    ImageBlitInfo CreateImageBlitInfo() const;
    ImageBlitInfo CreateImageBlitInfo(u32 mipBase, u32 layerBase, u32 layerCount) const;
    ImageBlitInfo CreateImageBlitInfo(const glm::uvec3& bottom, const glm::uvec3& top,
        u32 mipBase, u32 layerBase, u32 layerCount) const;

    ImageBindingInfo CreateBindingInfo(ImageFilter filter, ImageLayout layout) const;
    ImageBindingInfo CreateBindingInfo(Sampler sampler, ImageLayout layout) const;
    ImageBindingInfo CreateBindingInfo(ImageFilter filter, ImageLayout layout, ImageViewHandle handle) const;
    ImageBindingInfo CreateBindingInfo(Sampler sampler, ImageLayout layout, ImageViewHandle handle) const;

    static u16 CalculateMipmapCount(const glm::uvec2& resolution);
    static u16 CalculateMipmapCount(const glm::uvec3& resolution);
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

    static void CreateImageView(const ImageSubresource& imageSubresource,
        const std::vector<ImageSubresourceDescription>& additionalViews);

    ResourceHandle<Image> Handle() const { return m_ResourceHandle; }
private:
    ImageDescription m_Description{};
    ResourceHandle<Image> m_ResourceHandle;
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