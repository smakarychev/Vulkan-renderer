#pragma once

#include "ImageTraits.h"
#include "Sampler.h"
#include "Rendering/FormatTraits.h"
#include "Common/Span.h"

#include <string_view>
#include <variant>
#include <glm/glm.hpp>

struct LayoutTransitionInfo;

namespace assetLib
{
    enum class TextureFormat : u32;
}

struct ImageTag{};
using Image = ResourceHandleType<ImageTag>;

struct ImmediateSubmitContext;

class ImageViewHandle
{
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
    Image Image{};
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

enum class ImageSizeType
{
    Absolute, Relative,
};

// todo: once assets ready, change string_view to asset-handle
using ImageAssetPath = std::string_view;
using ImageDataSource = std::variant<ImageAssetPath, Span<const std::byte>, Image>;

struct ImageCreateInfo
{
    ImageDataSource DataSource{Span<const std::byte>{}};
    ImageDescription Description{};
    bool CalculateMipmaps{true};
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

    i8 mipmapCount(const glm::uvec2& resolution);
    i8 mipmapCount(const glm::uvec3& resolution);
    glm::uvec3 getPixelCoordinates(Image image, const glm::vec3& coordinate, ImageSizeType sizeType);
    
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
        static Texture Get(DefaultTexture texture);
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

