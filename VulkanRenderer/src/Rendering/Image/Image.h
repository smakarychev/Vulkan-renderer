#pragma once

#include "Rendering/Buffer.h"
#include "ImageTraits.h"
#include "Sampler.h"
#include "Rendering/FormatTraits.h"
#include "Rendering/SynchronizationTraits.h"

#include <string_view>
#include <unordered_map>
#include <variant>

struct LayoutTransitionInfo;

namespace assetLib
{
    enum class TextureFormat : u32;
}

struct ImmediateSubmitContext;
class Buffer;

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
    i8 MipmapBase{0};
    i8 Mipmaps{ALL_MIPMAPS};
    i8 LayerBase{0};
    i8 Layers{ALL_LAYERS};

    auto operator<=>(const ImageSubresourceDescription&) const = default;
};

struct ImageSubresource
{
    // todo: change to handle
    const Image* Image{nullptr};
    ImageSubresourceDescription Description{};
};

struct ImageDescription
{
    u32 Width{0};
    u32 Height{0};
    u32 LayersDepth{1};
    i8 Mipmaps{1};
    Format Format{Format::Undefined};
    ImageKind Kind{ImageKind::Image2d};
    ImageUsage Usage{ImageUsage::None};
    ImageFilter MipmapFilter{ImageFilter::Linear};
    std::vector<ImageSubresourceDescription> AdditionalViews{};

    glm::uvec3 Dimensions() const { return {Width, Height, GetDepth()}; }
    f32 AspectRatio() const { return (f32)Width / (f32)Height; }
    u32 GetDepth() const;
    i8 GetLayers() const;
};
using TextureDescription = ImageDescription;

struct ImageBlitInfo
{
    const Image* Image{nullptr};
    u32 MipmapBase{0};
    u32 LayerBase{0};
    u32 Layers{(u32)ImageSubresourceDescription::ALL_LAYERS};
    glm::uvec3 Bottom{};
    glm::uvec3 Top{};
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

// todo: once assets ready, change string_view to asset-handle
using ImageAssetPath = std::string_view;
using ImageDataSource = std::variant<ImageAssetPath, Span<const std::byte>, const Image*>;

struct ImageCreateInfo
{
    ImageDataSource DataSource{Span<const std::byte>{}};
    ImageDescription Description{};
    bool CalculateMipmaps{true};
};

class Image
{
    FRIEND_INTERNAL
public:
    static void Destroy(const Image& image);

    const ImageDescription& Description() const { return m_Description; }
    
    ImageBindingInfo BindingInfo(ImageFilter filter, ImageLayout layout) const;
    ImageBindingInfo BindingInfo(Sampler sampler, ImageLayout layout) const;
    ImageBindingInfo BindingInfo(ImageFilter filter, ImageLayout layout, ImageViewHandle handle) const;
    ImageBindingInfo BindingInfo(Sampler sampler, ImageLayout layout, ImageViewHandle handle) const;

    std::vector<ImageViewHandle> GetAdditionalViewHandles() const;
    ImageViewHandle GetViewHandle(ImageSubresourceDescription subresource) const;

    static i8 CalculateMipmapCount(const glm::uvec2& resolution);
    static i8 CalculateMipmapCount(const glm::uvec3& resolution);
    glm::uvec3 GetPixelCoordinate(const glm::vec3& coordinate, ImageSizeType sizeType) const;

    bool operator==(const Image& other) const { return m_ResourceHandle == other.m_ResourceHandle; }
    bool operator!=(const Image& other) const { return !(*this == other); }
private:
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

