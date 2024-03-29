#include "Image.h"

#include <numeric>

#include "Core/core.h"
#include "Vulkan/Driver.h"

#include "Buffer.h"
#include "Vulkan/RenderCommand.h"
#include "AssetLib.h"
#include "AssetManager.h"
#include "TextureAsset.h"

namespace
{
    constexpr Format formatFromAssetFormat(assetLib::TextureFormat format)
    {
        switch (format)
        {
        case assetLib::TextureFormat::Unknown:
            return Format::Undefined;
        case assetLib::TextureFormat::SRGBA8:
            return Format::RGBA8_SRGB;
        case assetLib::TextureFormat::RGBA8:
            return Format::RGBA8_UNORM;
        default:
            ASSERT(false, "Unsupported image format")
            break;
        }
        std::unreachable();
    }
}

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
        u32 white =     miniFloatOne        | miniFloatOne << 8     | miniFloatOne << 16    | miniFloatOne << 24;
        u32 black = 0;
        u32 red =       miniFloatOne                                                        | miniFloatOne << 24;
        u32 green =                          miniFloatOne << 8                              | miniFloatOne << 24;
        u32 blue =                                                    miniFloatOne << 16    | miniFloatOne << 24;
        u32 cyan =                           miniFloatOne << 8      | miniFloatOne << 16    | miniFloatOne << 24;
        u32 magenta =   miniFloatOne                                | miniFloatOne << 16    | miniFloatOne << 24;
        u32 yellow =    miniFloatOne        | miniFloatOne << 8                             | miniFloatOne << 24;

        u32 normal =    miniFloatOneHalf    | miniFloatOneHalf << 8 | miniFloatOne << 16    | miniFloatOne << 24;
        
        s_DefaultImages[(u32)DefaultTexture::White] = DefaultTextureData{
            .Texture = Texture::Builder(description)
                .FromPixels(std::vector{white})
                .Build(),
            .Color = white};

        s_DefaultImages[(u32)DefaultTexture::Black] = DefaultTextureData{
            .Texture = Texture::Builder(description)
                .FromPixels(std::vector{black})
                .Build(),
            .Color = black};
        
        s_DefaultImages[(u32)DefaultTexture::Red] = DefaultTextureData{
            .Texture = Texture::Builder(description)
                .FromPixels(std::vector{red})
                .Build(),
            .Color = red};
        s_DefaultImages[(u32)DefaultTexture::Green] = DefaultTextureData{
            .Texture = Texture::Builder(description)
                .FromPixels(std::vector{green})
                .Build(),
            .Color = green};
        s_DefaultImages[(u32)DefaultTexture::Blue] = DefaultTextureData{
            .Texture = Texture::Builder(description)
                .FromPixels(std::vector{blue})
                .Build(),
            .Color = blue};

        s_DefaultImages[(u32)DefaultTexture::Cyan] = DefaultTextureData{
            .Texture = Texture::Builder(description)
                .FromPixels(std::vector{cyan})
                .Build(),
            .Color = cyan};
        s_DefaultImages[(u32)DefaultTexture::Yellow] = DefaultTextureData{
            .Texture = Texture::Builder(description)
                .FromPixels(std::vector{yellow})
                .Build(),
            .Color = yellow};
        s_DefaultImages[(u32)DefaultTexture::Magenta] = DefaultTextureData{
            .Texture = Texture::Builder(description)
                .FromPixels(std::vector{magenta})
                .Build(),
            .Color = magenta};
        s_DefaultImages[(u32)DefaultTexture::NormalMap] = DefaultTextureData{
            .Texture = Texture::Builder(description)
                .FromPixels(std::vector{normal})
                .Build(),
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
        
        Texture copy =  Texture::Builder(textureOriginal.GetDescription())
            .FromPixels(std::vector{color})
            .Build(deletionQueue);

        return copy;
    }
}


Sampler Sampler::Builder::Build()
{
    return SamplerCache::CreateSampler(m_CreateInfo);
}

Sampler::Builder& Sampler::Builder::Filters(ImageFilter minification, ImageFilter magnification)
{
    m_CreateInfo.MinificationFilter = minification;
    m_CreateInfo.MagnificationFilter = magnification;

    return *this;
}

Sampler::Builder& Sampler::Builder::WrapMode(SamplerWrapMode mode)
{
    m_CreateInfo.AddressMode = mode;

    return *this;
}

Sampler::Builder& Sampler::Builder::ReductionMode(SamplerReductionMode mode)
{
    m_CreateInfo.ReductionMode = mode;

    return *this;
}

Sampler::Builder& Sampler::Builder::MaxLod(f32 lod)
{
    m_CreateInfo.MaxLod = lod;
    
    return *this;
}

Sampler::Builder& Sampler::Builder::WithAnisotropy(bool enabled)
{
    m_CreateInfo.WithAnisotropy = enabled;

    return *this;
}

Sampler Sampler::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void Sampler::Destroy(const Sampler& sampler)
{
    Driver::Destroy(sampler.Handle());
}

ImageSubresourceDescription::Packed ImageSubresourceDescription::Pack() const
{
    u8 mipBase = (u8)MipmapBase;
    // this should conveniently convert ALL_MIPMAPS to -1
    i8 mipmaps = (i8)Mipmaps;
    u8 layerBase = (u8)LayerBase;
    // this should conveniently convert ALL_LAYERS to -1
    i8 layers = (i8)Layers;

    Packed packed;
    packed.m_Data = mipBase | *(u8*)&mipmaps << 8 | layerBase << 16 | *(u8*)&layers << 24;
    
    return packed;
}

ImageSubresourceDescription::Packed ImageSubresourceDescription::Pack(const ImageSubresourceDescription& description)
{
    return description.Pack();
}

ImageSubresourceDescription ImageSubresourceDescription::Unpack(Packed packed)
{
    static constexpr u32 MASK = (1 << 8) - 1;
    u32 data = packed.m_Data;
    u8 mipBase = data & MASK;
    i8 mipmaps = (i8)(data >> 8 & MASK);
    u8 layerBase = data >> 16 & MASK;
    i8 layers = (i8)(data >> 24 & MASK);

    return {
        .MipmapBase = mipBase,
        .Mipmaps = (u32)mipmaps,
        .LayerBase = layerBase,
        .Layers = (u32)layers};
}

Image::Builder::Builder(const ImageDescription& description)
{
    m_CreateInfo.Description = description;
}

Image Image::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

Image Image::Builder::Build(DeletionQueue& deletionQueue)
{
    PreBuild();
    
    Image image = Image::Create(m_CreateInfo);
    if (!enumHasAny(image.m_Description.Usage, ImageUsage::NoDeallocation))
        deletionQueue.Enqueue(image);

    return image;
}

Image Image::Builder::BuildManualLifetime()
{
    PreBuild();
    
    return Image::Create(m_CreateInfo);
}

Image::Builder& Image::Builder::FromAssetFile(std::string_view path)
{
    m_CreateInfo.SourceInfo = CreateInfo::SourceInfo::Asset;
    m_CreateInfo.AssetInfo.AssetPath = path;

    Image* image = AssetManager::GetImage(m_CreateInfo.AssetInfo.AssetPath);
    if (image != nullptr)
    {
        m_CreateInfo.AssetInfo.Status = CreateInfo::AssetInfo::AssetStatus::Reused;
    }
    else
    {
        m_CreateInfo.AssetInfo.Status = CreateInfo::AssetInfo::AssetStatus::Loaded;

        assetLib::File textureFile;
        assetLib::loadAssetFile(path, textureFile);
        assetLib::TextureInfo textureInfo = assetLib::readTextureInfo(textureFile);
    
        m_CreateInfo.DataBuffer = Buffer::Builder()
            .SetUsage(BufferUsage::Source | BufferUsage::UploadRandomAccess)
            .SetSizeBytes(textureInfo.SizeBytes)
            .BuildManualLifetime();
    
        void* destination = m_CreateInfo.DataBuffer.Map();
        assetLib::unpackTexture(textureInfo, textureFile.Blob.data(), textureFile.Blob.size(), (u8*)destination);
        m_CreateInfo.DataBuffer.Unmap();
                    
        m_CreateInfo.Description.Format = formatFromAssetFormat(textureInfo.Format);
        m_CreateInfo.Description.Width = textureInfo.Dimensions.Width;
        m_CreateInfo.Description.Height = textureInfo.Dimensions.Height;
        // todo: not always correct, should reflect in in asset file
        m_CreateInfo.Description.Kind = ImageKind::Image2d;
        m_CreateInfo.Description.Layers = 1;
    }

    return *this;
}

Image::Builder& Image::Builder::FromPixels(const void* pixels, u64 sizeBytes)
{
    m_CreateInfo.SourceInfo = CreateInfo::SourceInfo::Pixels;

    m_CreateInfo.DataBuffer = Buffer::Builder()
        .SetUsage(BufferUsage::Source | BufferUsage::Upload)
        .SetSizeBytes(sizeBytes)
        .BuildManualLifetime();

    m_CreateInfo.DataBuffer.SetData(pixels, sizeBytes);

    return *this;
}

Image::Builder& Image::Builder::SetFormat(Format format)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset, "Cannot use custom format when loading from file")
    
    m_CreateInfo.Description.Format = format;
    
    return *this;
}

Image::Builder& Image::Builder::SetExtent(const glm::uvec2& extent)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset, "Cannot set extent when loading from file")

    m_CreateInfo.Description.Width = extent.x;
    m_CreateInfo.Description.Height = extent.y;
    m_CreateInfo.Description.Layers = 1;

    return *this;
}

Image::Builder& Image::Builder::SetExtent(const glm::uvec3& extent)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset, "Cannot set extent when loading from file")

    m_CreateInfo.Description.Width = extent.x;
    m_CreateInfo.Description.Height = extent.y;
    m_CreateInfo.Description.Layers = extent.z;
    
    return *this;
}

Image::Builder& Image::Builder::SetKind(ImageKind kind)
{
    m_CreateInfo.Description.Kind = kind;

    return *this;
}

Image::Builder& Image::Builder::CreateMipmaps(bool enable, ImageFilter filter)
{
    m_CreateInfo.CreateMipmaps = enable;
    m_CreateInfo.Description.MipmapFilter = filter;
    
    return *this;
}

Image::Builder& Image::Builder::SetUsage(ImageUsage usage)
{
    m_CreateInfo.Description.Usage = usage;

    return *this;
}

Image::Builder& Image::Builder::AddView(const ImageSubresourceDescription& subresource, ImageViewHandle& viewHandle)
{
    m_CreateInfo.Description.AdditionalViews.push_back(subresource.Pack());
    m_CreateInfo.AdditionalViews.push_back(subresource);
    // set index after push, so that it begins with 1. Index 0 is reserved for a base view
    viewHandle.m_Index = (u32)m_CreateInfo.AdditionalViews.size();

    return *this;
}

void Image::Builder::PreBuild()
{
    ASSERT(m_CreateInfo.Description.AdditionalViews.size() == m_CreateInfo.AdditionalViews.size(),
        "View count does not match the value specified in image description")
    
    if (m_CreateInfo.SourceInfo == CreateInfo::SourceInfo::Asset)
       m_CreateInfo.Description.Usage |= ImageUsage::Destination;
    
    if (m_CreateInfo.CreateMipmaps)
    {
        m_CreateInfo.Description.Usage |= ImageUsage::Destination | ImageUsage::Source;
        m_CreateInfo.Description.Mipmaps = CalculateMipmapCount({
            m_CreateInfo.Description.Width,
            m_CreateInfo.Description.Height,
            m_CreateInfo.Description.Kind == ImageKind::Image3d ? m_CreateInfo.Description.Layers : 1});
    }
    
    if (enumHasAny(m_CreateInfo.Description.Usage, ImageUsage::Readback))
        m_CreateInfo.Description.Usage |= ImageUsage::Source;
}

Image Image::Create(const Builder::CreateInfo& createInfo)
{
    Image image = {};
    
    switch (createInfo.SourceInfo)
    {
    case CreateInfo::SourceInfo::None:
    {
        image = AllocateImage(createInfo);
        CreateImageView(image.CreateSubresource(), createInfo.AdditionalViews);
        break;
    }
    case CreateInfo::SourceInfo::Asset:
    {
        image = CreateImageFromAsset(createInfo);
        break;
    }
    case CreateInfo::SourceInfo::Pixels:
    {
        image = CreateImageFromPixelData(createInfo);
        break;
    }
    default:
        ASSERT(false, "Unrecognized image source info")
        std::unreachable();
    }
    
    return image;
}

void Image::Destroy(const Image& image)
{
    Driver::Destroy(image.Handle());
}

ImageSubresource Image::CreateSubresource() const
{
    ImageSubresource imageSubresource = {
        .Image = this,
        .Description = {
            .MipmapBase = 0,
            .Mipmaps = m_Description.Mipmaps,
            .LayerBase = 0,
            .Layers = m_Description.Layers}};
    
    return imageSubresource;
}

ImageSubresource Image::CreateSubresource(u32 mipCount, u32 layerCount) const
{
    return CreateSubresource(0, mipCount, 0, layerCount);
}

ImageSubresource Image::CreateSubresource(u32 mipBase, u32 mipCount, u32 layerBase, u32 layerCount) const
{
    ImageSubresource imageSubresource = {
        .Image = this,
        .Description = {
            .MipmapBase = mipBase,
            .Mipmaps = mipCount,
            .LayerBase = layerBase,
            .Layers = layerCount}};

    return imageSubresource;
}

ImageSubresource Image::CreateSubresource(const ImageSubresourceDescription& description) const
{
    return ImageSubresource{
        .Image = this,
        .Description = description};
}

ImageBlitInfo Image::CreateImageBlitInfo() const
{
    return CreateImageBlitInfo(glm::uvec3{}, glm::uvec3{
        m_Description.Width,
        m_Description.Height,
        m_Description.Kind != ImageKind::Image3d ? 1u : m_Description.Layers},
        0, 0, m_Description.Kind != ImageKind::Image3d ? m_Description.Layers : 1u);
}

ImageBlitInfo Image::CreateImageBlitInfo(u32 mipBase, u32 layerBase, u32 layerCount) const
{
    return CreateImageBlitInfo(glm::uvec3{}, glm::uvec3{
        m_Description.Width,
        m_Description.Height,
        m_Description.Kind != ImageKind::Image3d ? 1u : m_Description.Layers},
        mipBase, layerBase, layerCount);
}

ImageBlitInfo Image::CreateImageBlitInfo(const glm::uvec3& bottom, const glm::uvec3& top, u32 mipBase, u32 layerBase,
    u32 layerCount) const
{
    return {
        .Image = this,
        .MipmapBase = mipBase,
        .LayerBase = layerBase,
        .Layers = layerCount,
        .Bottom = bottom,
        .Top = top};
}

ImageBlitInfo Image::CreateImageBlitInfo(const glm::vec3& bottom, const glm::vec3& top, u32 mipBase, u32 layerBase,
    u32 layerCount, ImageSizeType sizeType) const
{
    if (sizeType == ImageSizeType::Absolute)
        return {
            .Image = this,
            .MipmapBase = mipBase,
            .LayerBase = layerBase,
            .Layers = layerCount,
            .Bottom = glm::uvec3{bottom.x, bottom.y, bottom.z},
            .Top = glm::uvec3{top.x, top.y, top.z}};

    glm::uvec3 size = {
        m_Description.Width,
        m_Description.Height,
        m_Description.Kind != ImageKind::Image3d ? 1u : m_Description.Layers};

    glm::uvec3 absBottom = {
        (u32)((f32)size.x * bottom.x), (u32)((f32)size.y * bottom.y), (u32)((f32)size.z * bottom.z)};

    glm::uvec3 absTop = {
       (u32)((f32)size.x * top.x), (u32)((f32)size.y * top.y), (u32)((f32)size.z * top.z)};

    return {
        .Image = this,
        .MipmapBase = mipBase,
        .LayerBase = layerBase,
        .Layers = layerCount,
        .Bottom = glm::uvec3{absBottom.x, absBottom.y, absBottom.z},
        .Top = glm::uvec3{absTop.x, absTop.y, absTop.z}};
}

ImageBlitInfo Image::CreateImageCopyInfo() const
{
    return CreateImageBlitInfo();
}

ImageBlitInfo Image::CreateImageCopyInfo(u32 mipBase, u32 layerBase, u32 layerCount) const
{
    return CreateImageBlitInfo(mipBase, layerBase, layerCount);
}

ImageBlitInfo Image::CreateImageCopyInfo(const glm::uvec3& bottom, const glm::uvec3& size, u32 mipBase, u32 layerBase,
    u32 layerCount) const
{
    return CreateImageBlitInfo(bottom, size, mipBase, layerBase, layerCount);
}

ImageBlitInfo Image::CreateImageCopyInfo(const glm::vec3& bottom, const glm::vec3& size, u32 mipBase, u32 layerBase,
    u32 layerCount, ImageSizeType sizeType) const
{
    return CreateImageBlitInfo(bottom, size, mipBase, layerBase, layerCount, sizeType);
}

ImageBindingInfo Image::CreateBindingInfo(ImageFilter filter, ImageLayout layout) const
{
    return CreateBindingInfo(Sampler::Builder().Filters(filter, filter).Build(), layout);
}

ImageBindingInfo Image::CreateBindingInfo(Sampler sampler, ImageLayout layout) const
{
    return ImageBindingInfo {
        .Image = this,
        .Sampler = sampler,
        .Layout = layout};
}

ImageBindingInfo Image::CreateBindingInfo(ImageFilter filter, ImageLayout layout, ImageViewHandle handle) const
{
    return CreateBindingInfo(Sampler::Builder().Filters(filter, filter).Build(), layout, handle);
}

ImageBindingInfo Image::CreateBindingInfo(Sampler sampler, ImageLayout layout, ImageViewHandle handle) const
{
    return ImageBindingInfo {
        .Image = this,
        .Sampler = sampler,
        .Layout = layout,
        .ViewHandle = handle};
}

std::vector<ImageViewHandle> Image::GetViewHandles() const
{
    std::vector<ImageViewHandle> handles(m_Description.AdditionalViews.size());
    for (u32 i = 0; i < m_Description.AdditionalViews.size(); i++)
        handles[i] = i;
    
    return handles;
}

u16 Image::CalculateMipmapCount(const glm::uvec2& resolution)
{
    return CalculateMipmapCount({resolution.x, resolution.y, 1});
}

u16 Image::CalculateMipmapCount(const glm::uvec3& resolution)
{
    u32 maxDimension = std::max(resolution.x, std::max(resolution.y, resolution.z));

    return (u16)std::log2(maxDimension) + 1;    
}

Image Image::CreateImageFromAsset(const CreateInfo& createInfo)
{
    Image image = {};
    
    if (createInfo.AssetInfo.Status == CreateInfo::AssetInfo::AssetStatus::Reused)
    {
        image = *AssetManager::GetImage(createInfo.AssetInfo.AssetPath);
        image.m_Description.Usage |= ImageUsage::NoDeallocation; // to avoid multiple deletions of the same texture
    }
    else
    {
        image = CreateImageFromBuffer(createInfo);
        AssetManager::AddImage(createInfo.AssetInfo.AssetPath, image);
    }

    return image;
}

Image Image::CreateImageFromPixelData(const CreateInfo& createInfo)
{
    return CreateImageFromBuffer(createInfo);
}

Image Image::CreateImageFromBuffer(const CreateInfo& createInfo)
{
    Image image = {};
    
    image = AllocateImage(createInfo);
    ImageSubresource imageSubresource = image.CreateSubresource(0, 1, 0, 1);
    PrepareForMipmapDestination(imageSubresource);
    CopyBufferToImage(createInfo.DataBuffer, image);
    imageSubresource.Description.Mipmaps = createInfo.Description.Mipmaps;
    CreateMipmaps(image, createInfo);
    PrepareForShaderRead(imageSubresource);
    
    CreateImageView(image.CreateSubresource(), createInfo.AdditionalViews);
        
    return image;
}

Image Image::AllocateImage(const CreateInfo& createInfo)
{
    return Driver::AllocateImage(createInfo);
}

void Image::PrepareForMipmapDestination(const ImageSubresource& imageSubresource)
{
    PrepareImageGeneral(imageSubresource,
        ImageLayout::Undefined, ImageLayout::Destination,
        PipelineAccess::None, PipelineAccess::WriteTransfer,
        PipelineStage::AllTransfer, PipelineStage::AllTransfer);
}

void Image::PrepareForMipmapSource(const ImageSubresource& imageSubresource)
{
    PrepareImageGeneral(imageSubresource,
        ImageLayout::Destination, ImageLayout::Source,
        PipelineAccess::WriteTransfer, PipelineAccess::ReadTransfer,
        PipelineStage::AllTransfer, PipelineStage::AllTransfer);
}

void Image::PrepareForShaderRead(const ImageSubresource& imageSubresource)
{
    ImageLayout current = imageSubresource.Description.Mipmaps > 1 ? ImageLayout::Source : ImageLayout::Destination;
    PrepareImageGeneral(imageSubresource,
       current, ImageLayout::Readonly,
       PipelineAccess::ReadTransfer, PipelineAccess::ReadShader,
       PipelineStage::AllTransfer, PipelineStage::PixelShader);
}

void Image::PrepareImageGeneral(const ImageSubresource& imageSubresource,
    ImageLayout current, ImageLayout target,
    PipelineAccess srcAccess, PipelineAccess dstAccess,
    PipelineStage srcStage, PipelineStage dstStage)
{
    DeletionQueue deletionQueue = {};
    
    DependencyInfo layoutTransition = DependencyInfo::Builder()
        .LayoutTransition({
            .ImageSubresource = imageSubresource,
            .SourceStage = srcStage,
            .DestinationStage = dstStage,
            .SourceAccess = srcAccess,
            .DestinationAccess = dstAccess,
            .OldLayout = current,
            .NewLayout = target})
        .Build(deletionQueue);
    
    Driver::ImmediateUpload([&layoutTransition](const CommandBuffer& cmd)
    {
       RenderCommand::WaitOnBarrier(cmd, layoutTransition);
    });
}

void Image::CopyBufferToImage(const Buffer& buffer, const Image& image)
{
    Driver::ImmediateUpload([&buffer, &image](const CommandBuffer& cmd)
    {
        RenderCommand::CopyBufferToImage(cmd, buffer, image.CreateSubresource(0, 1, 0, image.m_Description.Layers));
    });
    
    Buffer::Destroy(buffer);
}

void Image::CreateMipmaps(const Image& image, const CreateInfo& createInfo)
{
    if (createInfo.Description.Mipmaps == 1)
        return;

    bool is3dImage = createInfo.Description.Kind == ImageKind::Image3d;
    u32 layerCount = is3dImage ? createInfo.Description.Layers : 1;
    ImageSubresource imageSubresource = image.CreateSubresource(0, 1, 0, layerCount);
    imageSubresource.Description.Layers = layerCount;
    imageSubresource.Description.Mipmaps = 1;
    
    PrepareForMipmapSource(imageSubresource);

    for (u32 i = 1; i < createInfo.Description.Mipmaps; i++)
    {
        ImageBlitInfo source = image.CreateImageBlitInfo({},
            {
            (i32)createInfo.Description.Width >> (i - 1),
            (i32)createInfo.Description.Height >> (i - 1),
            is3dImage ? (i32)createInfo.Description.Layers >> (i - 1) : 1},
            i - 1, 0, layerCount);

        ImageBlitInfo destination = image.CreateImageBlitInfo({},
            {
            (i32)createInfo.Description.Width >> i,
            (i32)createInfo.Description.Height >> i,
            is3dImage ? (i32)createInfo.Description.Layers >> i : 1},
            i, 0, layerCount);
        
        ImageSubresource mipmapSubresource = image.CreateSubresource(i, 1, 0, layerCount);

        PrepareForMipmapDestination(mipmapSubresource);
    
        Driver::ImmediateUpload([&image, &source, destination](const CommandBuffer& cmd)
        {
                RenderCommand::BlitImage(cmd, source, destination, image.m_Description.MipmapFilter);
        });
        PrepareForMipmapSource(mipmapSubresource);
    }
}

void Image::CreateImageView(const ImageSubresource& imageSubresource,
    const std::vector<ImageSubresourceDescription>& additionalViews)
{
    ASSERT(imageSubresource.Description.MipmapBase + imageSubresource.Description.Mipmaps <=
        imageSubresource.Image->m_Description.Mipmaps,
        "Incorrect mipmap range for image view")
    ASSERT(imageSubresource.Description.LayerBase + imageSubresource.Description.Layers <=
        imageSubresource.Image->m_Description.Layers,
        "Incorrect layer range for image view")

    Driver::CreateViews(imageSubresource, additionalViews);
}

Sampler SamplerCache::CreateSampler(const Sampler::Builder::CreateInfo& createInfo)
{
    CacheKey key = {.CreateInfo = createInfo};

    if (s_SamplerCache.contains(key))
        return s_SamplerCache.at(key);

    Sampler newSampler = Sampler::Create(createInfo);
    s_SamplerCache.emplace(key, newSampler);
    
    Driver::DeletionQueue().Enqueue(newSampler);

    return newSampler;
}

bool SamplerCache::CacheKey::operator==(const CacheKey& other) const
{
    return
        CreateInfo.MinificationFilter == other.CreateInfo.MinificationFilter &&
        CreateInfo.MagnificationFilter == other.CreateInfo.MagnificationFilter &&
        CreateInfo.ReductionMode == other.CreateInfo.ReductionMode &&
        CreateInfo.MaxLod == other.CreateInfo.MaxLod &&
        CreateInfo.WithAnisotropy == other.CreateInfo.WithAnisotropy;
}

u64 SamplerCache::SamplerKeyHash::operator()(const CacheKey& cacheKey) const
{
    u64 hashKey =
        (u32)cacheKey.CreateInfo.MagnificationFilter |
        ((u32)cacheKey.CreateInfo.MagnificationFilter << 1) |
        (cacheKey.CreateInfo.WithAnisotropy << 2) |
        (cacheKey.CreateInfo.ReductionMode.has_value() << 3) |
        ((cacheKey.CreateInfo.ReductionMode.has_value() ? (u32)*cacheKey.CreateInfo.ReductionMode : 0) << 4) |
        ((u64)std::bit_cast<u32>(cacheKey.CreateInfo.MaxLod) << 32);
    u64 hash = std::hash<u64>()(hashKey);
    
    return hash;
}

std::unordered_map<SamplerCache::CacheKey, Sampler, SamplerCache::SamplerKeyHash> SamplerCache::s_SamplerCache = {};
