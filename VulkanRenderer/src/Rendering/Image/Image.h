#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>

#include "Rendering/Buffer.h"
#include "ImageTraits.h"
#include "Sampler.h"
#include "Rendering/FormatTraits.h"
#include "Rendering/SynchronizationTraits.h"

struct LayoutTransitionInfo;

namespace assetLib
{
    enum class TextureFormat : u32;
}

struct ImmediateSubmitContext;
class Buffer;
class Swapchain;
class Device;

class ImageViewHandle
{
    friend class Image;
    FRIEND_INTERNAL
public:
    ImageViewHandle() = default;
private:
    ImageViewHandle(u32 index) : m_Index(index) {}
private:
    u32 m_Index{0};
};

struct ImageSubresourceDescription
{
    static constexpr i8 ALL_MIPMAPS = -1;
    static constexpr i8 ALL_LAYERS = -1;

    ImageViewKind ImageViewKind{ImageViewKind::Inherit};
    u8 MipmapBase;
    i8 Mipmaps{ALL_MIPMAPS};
    u8 LayerBase;
    i8 Layers{ALL_LAYERS};

    auto operator<=>(const ImageSubresourceDescription&) const = default;
};

struct ImageSubresource
{
    const Image* Image;
    ImageSubresourceDescription Description;
};

struct ImageDescription
{
    u32 Width{0};
    u32 Height{0};
    u32 Layers{1};
    i8 Mipmaps{1};
    Format Format{Format::Undefined};
    ImageKind Kind{ImageKind::Image2d};
    ImageUsage Usage{ImageUsage::None};
    ImageFilter MipmapFilter{ImageFilter::Linear};
    std::vector<ImageSubresourceDescription> AdditionalViews{};

    f32 AspectRatio() const { return (f32)Width / (f32)Height; }

    static u32 GetDepth(const ImageDescription& description);
    static i8 GetLayers(const ImageDescription& description);
};
using TextureDescription = ImageDescription;

struct ImageBlitInfo
{
    const Image* Image;
    u32 MipmapBase;
    u32 LayerBase;
    u32 Layers{(u32)ImageSubresourceDescription::ALL_LAYERS};
    glm::uvec3 Bottom;
    glm::uvec3 Top;
};
using ImageCopyInfo = ImageBlitInfo;

struct ImageBindingInfo
{
    const Image* Image;
    Sampler Sampler;
    ImageLayout Layout;
    
    ImageViewHandle ViewHandle{};
};
using TextureBindingInfo = ImageBindingInfo;

enum class ImageSizeType
{
    Absolute, Relative,
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
            enum class SourceInfo {None, Asset, Pixels, Equirectangular};
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
            std::vector<ImageSubresourceDescription> AdditionalViews;
            bool NoMips{false};
        };
    public:
        Builder() = default;
        Builder(const ImageDescription& description);
        Image Build();
        Image Build(DeletionQueue& deletionQueue);
        Image BuildManualLifetime();
        Builder& FromEquirectangular(std::string_view path);
        Builder& FromAssetFile(std::string_view path);
        template <typename T>
        Builder& FromPixels(const std::vector<T>& pixels)
        {
            return FromPixels(pixels.data(), pixels.size() * sizeof(T));
        }
        /* builder should not create mipmaps (still allocates if mipmap count is not 1),
         * intended for the cases when mipmap creation has to be delayed (when image does not yet have any pixel data)
         */
        Builder& NoMips();
    private:
        void PreBuild();
        Builder& FromPixels(const void* pixels, u64 sizeBytes);
        void SetSource(enum CreateInfo::SourceInfo sourceInfo);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Image Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Image& image);

    const ImageDescription& Description() const { return m_Description; }
    
    ImageSubresource Subresource() const;
    ImageSubresource Subresource(i8 mipCount, i8 layerCount) const;
    ImageSubresource Subresource(u8 mipBase, i8 mipCount, u8 layerBase, i8 layerCount) const;
    ImageSubresource Subresource(const ImageSubresourceDescription& description) const;

    ImageBlitInfo BlitInfo() const;
    ImageBlitInfo BlitInfo(u32 mipBase, u32 layerBase, u32 layerCount) const;
    ImageBlitInfo BlitInfo(const glm::uvec3& bottom, const glm::uvec3& top,
        u32 mipBase, u32 layerBase, u32 layerCount) const;
    
    ImageBlitInfo CopyInfo() const;
    ImageBlitInfo CopyInfo(u32 mipBase, u32 layerBase, u32 layerCount) const;
    ImageBlitInfo CopyInfo(const glm::uvec3& bottom, const glm::uvec3& size,
        u32 mipBase, u32 layerBase, u32 layerCount) const;

    ImageBindingInfo BindingInfo(ImageFilter filter, ImageLayout layout) const;
    ImageBindingInfo BindingInfo(Sampler sampler, ImageLayout layout) const;
    ImageBindingInfo BindingInfo(ImageFilter filter, ImageLayout layout, ImageViewHandle handle) const;
    ImageBindingInfo BindingInfo(Sampler sampler, ImageLayout layout, ImageViewHandle handle) const;

    std::vector<ImageViewHandle> GetAdditionalViewHandles() const;
    ImageViewHandle GetViewHandle(ImageSubresourceDescription subresource) const;

    static i8 CalculateMipmapCount(const glm::uvec2& resolution);
    static i8 CalculateMipmapCount(const glm::uvec3& resolution);
    void CreateMipmaps(ImageLayout currentLayout);

    glm::uvec3 GetPixelCoordinate(const glm::vec3& coordinate, ImageSizeType sizeType) const;

    bool operator==(const Image& other) const { return m_ResourceHandle == other.m_ResourceHandle; }
    bool operator!=(const Image& other) const { return !(*this == other); }
private:
    using CreateInfo = Builder::CreateInfo;
    static Image CreateImage(const CreateInfo& createInfo);
    static Image CreateImageFromAsset(const CreateInfo& createInfo);
    static Image CreateImageFromEquirectangular(const CreateInfo& createInfo);
    static Image CreateImageFromPixelData(const CreateInfo& createInfo);
    static Image CreateImageFromBuffer(const CreateInfo& createInfo);
    static Image AllocateImage(const CreateInfo& createInfo);
    static void PrepareForMipmapDestination(const ImageSubresource& imageSubresource);
    static void PrepareForMipmapSource(const ImageSubresource& imageSubresource, ImageLayout currentLayout);
    static void PrepareForShaderRead(const ImageSubresource& imageSubresource);
    static void PrepareImageGeneral(const ImageSubresource& imageSubresource,
        ImageLayout current, ImageLayout target,
        PipelineAccess srcAccess, PipelineAccess dstAccess,
        PipelineStage srcStage, PipelineStage dstStage);
    static void CopyBufferToImage(const Buffer& buffer, const Image& image);

    static void CreateImageView(const ImageSubresource& imageSubresource,
        const std::vector<ImageSubresourceDescription>& additionalViews);

    ResourceHandleType<Image> Handle() const { return m_ResourceHandle; }
private:
    ImageDescription m_Description{};
    ResourceHandleType<Image> m_ResourceHandle{};
};

using Texture = Image;

namespace ImageUtils
{
    std::string imageKindToString(ImageKind kind);
    std::string imageViewKindToString(ImageViewKind kind);
    std::string imageUsageToString(ImageUsage usage);
    std::string imageFilterToString(ImageFilter filter);
    std::string imageLayoutToString(ImageLayout layout);

    u32 toRGBA8(const glm::vec4& color);
    u32 toRGBA8SNorm(const glm::vec4& color);

    enum class DefaultTexture
    {
        White = 0, Black, Red, Green, Blue, Cyan, Yellow, Magenta,
        NormalMap,
        MaxVal
    };
    class DefaultTextures
    {
    public:
        static void Init();
        static const Texture& Get(DefaultTexture texture);
        static Texture GetCopy(DefaultTexture texture, DeletionQueue& deletionQueue);
    private:
        struct DefaultTextureData
        {
            Texture Texture;
            u32 Color;
        };
        static std::array<DefaultTextureData, (u32)DefaultTexture::MaxVal> s_DefaultImages;
    };
}

