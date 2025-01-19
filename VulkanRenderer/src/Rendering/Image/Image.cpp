#include "Image.h"

#include "core.h"
#include "Vulkan/Device.h"

#include "Vulkan/RenderCommand.h"
#include "AssetManager.h"

namespace ImageUtils
{
    std::string imageKindToString(ImageKind kind)
    {
        switch (kind)
        {
        case ImageKind::Image2d:
            return "Image2d";
        case ImageKind::Image3d:
            return "Image3d";
        case ImageKind::Cubemap:
            return "Cubemap";
        case ImageKind::Image2dArray:
            return "Image2dArray";
        default:
            return "";
        }
    }

    std::string imageViewKindToString(ImageViewKind kind)
    {
        switch (kind)
        {
        case ImageViewKind::Inherit:
            return "Inherit";
        case ImageViewKind::Image2d:
            return "Image2d";
        case ImageViewKind::Image3d:
            return "Image3d";
        case ImageViewKind::Cubemap:
            return "Cubemap";
        case ImageViewKind::Image2dArray:
            return "Image2dArray";
        default:
            return "";
        }
    }

    std::string imageUsageToString(ImageUsage usage)
    {
        std::string usageString;
        if (enumHasAny(usage, ImageUsage::Sampled))
            usageString += usageString.empty() ? "Sampled" : " | Sampled";
        if (enumHasAny(usage, ImageUsage::Color))
            usageString += usageString.empty() ? "Color" : " | Color";
        if (enumHasAny(usage, ImageUsage::Depth))
            usageString += usageString.empty() ? "Depth" : " | Depth";
        if (enumHasAny(usage, ImageUsage::Stencil))
            usageString += usageString.empty() ? "Stencil" : " | Stencil";
        if (enumHasAny(usage, ImageUsage::Storage))
            usageString += usageString.empty() ? "Storage" : " | Storage";
        if (enumHasAny(usage, ImageUsage::Readback))
            usageString += usageString.empty() ? "Readback" : " | Readback";
        if (enumHasAny(usage, ImageUsage::Source))
            usageString += usageString.empty() ? "Source" : " | Source";
        if (enumHasAny(usage, ImageUsage::Destination))
            usageString += usageString.empty() ? "Destination" : " | Destination";
        if (enumHasAny(usage, ImageUsage::NoDeallocation))
            usageString += usageString.empty() ? "NoDeallocation" : " | NoDeallocation";

        return usageString;
    }

    std::string imageFilterToString(ImageFilter filter)
    {
        switch (filter)
        {
        case ImageFilter::Linear:
            return "Linear";
        case ImageFilter::Nearest:
            return "Nearest";
        default:
            return "";
        }
    }

    std::string imageLayoutToString(ImageLayout layout)
    {
        switch (layout)
        {
        case ImageLayout::Undefined:                return "Undefined"; 
        case ImageLayout::General:                  return "General"; 
        case ImageLayout::Attachment:               return "Attachment"; 
        case ImageLayout::Readonly:                 return "Readonly"; 
        case ImageLayout::ColorAttachment:          return "ColorAttachment"; 
        case ImageLayout::Present:                  return "Present"; 
        case ImageLayout::DepthStencilAttachment:   return "DepthStencilAttachment"; 
        case ImageLayout::DepthStencilReadonly:     return "DepthStencilReadonly"; 
        case ImageLayout::DepthAttachment:          return "DepthAttachment"; 
        case ImageLayout::DepthReadonly:            return "DepthReadonly"; 
        case ImageLayout::Source:                   return "Source"; 
        case ImageLayout::Destination:              return "Destination";
        default:                                    return "";
        } 
    }

    u32 toRGBA8(const glm::vec4& color)
    {
        u8 r = (u8)(color.r * 255.0f);
        u8 g = (u8)(color.g * 255.0f);
        u8 b = (u8)(color.b * 255.0f);
        u8 a = (u8)(color.a * 255.0f);

        return r | g << 8 | b << 16 | a << 24;
    }

    u32 toRGBA8SNorm(const glm::vec4& color)
    {
        i8 r = (i8)(color.r * 127.0f);
        i8 g = (i8)(color.g * 127.0f);
        i8 b = (i8)(color.b * 127.0f);
        i8 a = (i8)(color.a * 127.0f);

        return *(u8*)&r | *(u8*)&g << 8 | *(u8*)&b << 16 | *(u8*)&a << 24;
    }

    std::array<DefaultTextures::DefaultTextureData, (u32)DefaultTexture::MaxVal> DefaultTextures::s_DefaultImages = {};
    
    void DefaultTextures::Init()
    {
        ImageDescription description = {
            .Width = 1,
            .Height = 1,
            .Format = Format::RGBA8_SNORM,
            .Kind = ImageKind::Image2d,
            .Usage = ImageUsage::Sampled | ImageUsage::Source | ImageUsage::Destination};
        
        u32 miniFloatOne = 0b0'1111'111;
        u32 miniFloatOneHalf = 0b0'0111'111;
        u32 white   =   miniFloatOne        | miniFloatOne << 8     | miniFloatOne << 16    | miniFloatOne << 24;
        u32 black   =   0;
        u32 red     =   miniFloatOne                                                        | miniFloatOne << 24;
        u32 green   =                         miniFloatOne << 8                             | miniFloatOne << 24;
        u32 blue    =                                                 miniFloatOne << 16    | miniFloatOne << 24;
        u32 cyan    =                         miniFloatOne << 8     | miniFloatOne << 16    | miniFloatOne << 24;
        u32 magenta =   miniFloatOne                                | miniFloatOne << 16    | miniFloatOne << 24;
        u32 yellow  =   miniFloatOne        | miniFloatOne << 8                             | miniFloatOne << 24;

        u32 normal  =   miniFloatOneHalf    | miniFloatOneHalf << 8 | miniFloatOne << 16    | miniFloatOne << 24;
        
        s_DefaultImages[(u32)DefaultTexture::White] = DefaultTextureData{
            .Texture = Device::CreateImage({
                .DataSource = Span<const std::byte>({white}),
                .Description = description}),
            .Color = white};

        s_DefaultImages[(u32)DefaultTexture::Black] = DefaultTextureData{
            .Texture = Device::CreateImage({
                .DataSource = Span<const std::byte>({black}),
                .Description = description}),
            .Color = black};
        
        s_DefaultImages[(u32)DefaultTexture::Red] = DefaultTextureData{
            .Texture = Device::CreateImage({
                .DataSource = Span<const std::byte>({red}),
                .Description = description}),
            .Color = red};
        s_DefaultImages[(u32)DefaultTexture::Green] = DefaultTextureData{
            .Texture = Device::CreateImage({
                .DataSource = Span<const std::byte>({green}),
                .Description = description}),
            .Color = green};
        s_DefaultImages[(u32)DefaultTexture::Blue] = DefaultTextureData{
            .Texture = Device::CreateImage({
                .DataSource = Span<const std::byte>({blue}),
                .Description = description}),
            .Color = blue};

        s_DefaultImages[(u32)DefaultTexture::Cyan] = DefaultTextureData{
            .Texture = Device::CreateImage({
                .DataSource = Span<const std::byte>({cyan}),
                .Description = description}),
            .Color = cyan};
        s_DefaultImages[(u32)DefaultTexture::Yellow] = DefaultTextureData{
            .Texture = Device::CreateImage({
                .DataSource = Span<const std::byte>({yellow}),
                .Description = description}),
            .Color = yellow};
        s_DefaultImages[(u32)DefaultTexture::Magenta] = DefaultTextureData{
            .Texture = Device::CreateImage({
                .DataSource = Span<const std::byte>({magenta}),
                .Description = description}),
            .Color = magenta};
        s_DefaultImages[(u32)DefaultTexture::NormalMap] = DefaultTextureData{
            .Texture = Device::CreateImage({
                .DataSource = Span<const std::byte>({normal}),
                .Description = description}),
            .Color = normal};
    }

    const Texture& DefaultTextures::Get(DefaultTexture texture)
    {
        ASSERT((u32)texture < (u32)DefaultTexture::MaxVal, "Incorrect texture type")

        return s_DefaultImages[(u32)texture].Texture;
    }

    Texture DefaultTextures::GetCopy(DefaultTexture texture, DeletionQueue& deletionQueue)
    {
        ASSERT((u32)texture < (u32)DefaultTexture::MaxVal, "Incorrect texture type")

        const auto& [textureOriginal, color] = s_DefaultImages[(u32)texture];
        
        Texture copy = Device::CreateImage({
            .DataSource = Span<const std::byte>({color}),
            .Description = textureOriginal.Description()},
            deletionQueue);

        return copy;
    }
}

u32 ImageDescription::GetDepth() const
{
    const bool is3dImage = Kind == ImageKind::Image3d;
    return is3dImage ? LayersDepth : 1;
}

i8 ImageDescription::GetLayers() const
{
    const bool is3dImage = Kind == ImageKind::Image3d;
    return is3dImage ? (i8)1 : (i8)LayersDepth;
}

void Image::Destroy(const Image& image)
{
    Device::Destroy(image.Handle());
}

ImageBindingInfo Image::BindingInfo(ImageFilter filter, ImageLayout layout) const
{
    return BindingInfo(
        Device::CreateSampler({
            .MinificationFilter = filter,
            .MagnificationFilter = filter}),
        layout);
}

ImageBindingInfo Image::BindingInfo(Sampler sampler, ImageLayout layout) const
{
    return ImageBindingInfo {
        .Image = this,
        .Sampler = sampler,
        .Layout = layout};
}

ImageBindingInfo Image::BindingInfo(ImageFilter filter, ImageLayout layout, ImageViewHandle handle) const
{
    return BindingInfo(
        Device::CreateSampler({
            .MinificationFilter = filter,
            .MagnificationFilter = filter}),
        layout,
        handle);
}

ImageBindingInfo Image::BindingInfo(Sampler sampler, ImageLayout layout, ImageViewHandle handle) const
{
    return ImageBindingInfo {
        .Image = this,
        .Sampler = sampler,
        .Layout = layout,
        .ViewHandle = handle};
}

std::vector<ImageViewHandle> Image::GetAdditionalViewHandles() const
{
    std::vector<ImageViewHandle> handles(m_Description.AdditionalViews.size());
    for (u32 i = 0; i < m_Description.AdditionalViews.size(); i++)
        handles[i] = i + 1; // skip index 0, it is used as a base view
    
    return handles;
}

ImageViewHandle Image::GetViewHandle(ImageSubresourceDescription subresource) const
{
    if (subresource == ImageSubresourceDescription{})
        return 0;
        
    auto it = std::ranges::find(m_Description.AdditionalViews, subresource);

    if (it != m_Description.AdditionalViews.end())
        return ImageViewHandle{u32(it - m_Description.AdditionalViews.begin()) + 1};
    
    LOG("ERROR: Image does not have such view subresource, returning default view");
    return ImageViewHandle{};
}

i8 Image::CalculateMipmapCount(const glm::uvec2& resolution)
{
    return CalculateMipmapCount({resolution.x, resolution.y, 1});
}

i8 Image::CalculateMipmapCount(const glm::uvec3& resolution)
{
    u32 maxDimension = std::max(resolution.x, std::max(resolution.y, resolution.z));

    return (i8)std::max(1, (i8)std::log2(maxDimension) + (i8)!MathUtils::isPowerOf2(maxDimension));    
}

glm::uvec3 Image::GetPixelCoordinate(const glm::vec3& coordinate, ImageSizeType sizeType) const
{
    if (sizeType == ImageSizeType::Absolute)
        return glm::uvec3{coordinate};

    glm::uvec3 size = {
        m_Description.Width,
        m_Description.Height,
        m_Description.GetDepth()};

    return glm::uvec3 {
        (u32)((f32)size.x * coordinate.x), (u32)((f32)size.y * coordinate.y), (u32)((f32)size.z * coordinate.z)};
}