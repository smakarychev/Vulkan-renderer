#include "Image.h"

#include "Core/core.h"
#include "Vulkan/Device.h"

#include "Rendering/Buffer.h"
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
        case assetLib::TextureFormat::RGBA32:
            return Format::RGBA32_FLOAT;
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
        
        Texture copy =  Texture::Builder(textureOriginal.Description())
            .FromPixels(std::vector{color})
            .Build(deletionQueue);

        return copy;
    }
}

u32 ImageDescription::GetDepth(const ImageDescription& description)
{
    const bool is3dImage = description.Kind == ImageKind::Image3d;
    return is3dImage ? description.Layers : 1;
}

i8 ImageDescription::GetLayers(const ImageDescription& description)
{
    const bool is3dImage = description.Kind == ImageKind::Image3d;
    return is3dImage ? (i8)1 : (i8)description.Layers;
}

Image::Builder::Builder(const ImageDescription& description)
{
    m_CreateInfo.Description = description;
}

Image Image::Builder::Build()
{
    return Build(Device::DeletionQueue());
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

Image::Builder& Image::Builder::FromEquirectangular(const Image& texture)
{
    // Equirectangular images have to be converted to cubemaps before using them in shader,
    // Here we only create an empty cube map, it has to be generated using Passes::EquirectangularToCubemap

    SetSource(CreateInfo::SourceInfo::Equirectangular);

    u32 textureHeight = texture.Description().Height / 2;
    u32 textureWidth = textureHeight;
    m_CreateInfo.Description.Width = textureWidth;
    m_CreateInfo.Description.Height = textureHeight;
    m_CreateInfo.Description.Layers = 6;
    m_CreateInfo.Description.Format = Format::RGBA16_FLOAT;
    m_CreateInfo.Description.Kind = ImageKind::Cubemap;
    m_CreateInfo.Description.Mipmaps = CalculateMipmapCount({textureWidth, textureHeight});
    m_CreateInfo.NoMips = true;

    return *this;
}

Image::Builder& Image::Builder::FromAssetFile(std::string_view path)
{
    SetSource(CreateInfo::SourceInfo::Asset);
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
    
        m_CreateInfo.DataBuffer = Device::CreateBuffer({
            .SizeBytes = textureInfo.SizeBytes,
            .Usage = BufferUsage::Source | BufferUsage::StagingRandomAccess});
    
        void* destination = m_CreateInfo.DataBuffer.Map();
        assetLib::unpackTexture(textureInfo, textureFile.Blob.data(), textureFile.Blob.size(), (u8*)destination);
        m_CreateInfo.DataBuffer.Unmap();
                    
        m_CreateInfo.Description.Format = formatFromAssetFormat(textureInfo.Format);
        m_CreateInfo.Description.Width = textureInfo.Dimensions.Width;
        m_CreateInfo.Description.Height = textureInfo.Dimensions.Height;
        m_CreateInfo.Description.Mipmaps = CalculateMipmapCount({
            textureInfo.Dimensions.Width, textureInfo.Dimensions.Height});
        // todo: not always correct, should reflect in in asset file
        m_CreateInfo.Description.Kind = ImageKind::Image2d;
        m_CreateInfo.Description.Layers = 1;
    }

    return *this;
}

Image::Builder& Image::Builder::FromPixels(const void* pixels, u64 sizeBytes)
{
    SetSource(CreateInfo::SourceInfo::Pixels);

    m_CreateInfo.DataBuffer = Device::CreateBuffer({
            .SizeBytes = sizeBytes,
            .Usage = BufferUsage::Source | BufferUsage::Staging});

    m_CreateInfo.DataBuffer.SetData(Span{(std::byte*)pixels, sizeBytes});

    return *this;
}

void Image::Builder::SetSource(enum CreateInfo::SourceInfo sourceInfo)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset || sourceInfo == CreateInfo::SourceInfo::Asset)
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Pixels || sourceInfo == CreateInfo::SourceInfo::Pixels)
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Equirectangular ||
        sourceInfo == CreateInfo::SourceInfo::Equirectangular)

    m_CreateInfo.SourceInfo = sourceInfo;
}

Image::Builder& Image::Builder::NoMips()
{
    m_CreateInfo.NoMips = true;
    
    return *this;
}

void Image::Builder::PreBuild()
{
    if (m_CreateInfo.SourceInfo == CreateInfo::SourceInfo::Asset)
        m_CreateInfo.Description.Usage |= ImageUsage::Destination;
    
    if (m_CreateInfo.Description.Mipmaps > 1)
        m_CreateInfo.Description.Usage |= ImageUsage::Destination | ImageUsage::Source;

    if (m_CreateInfo.Description.Kind == ImageKind::Cubemap)
        m_CreateInfo.Description.Layers = 6;
    
    if (enumHasAny(m_CreateInfo.Description.Usage, ImageUsage::Readback))
        m_CreateInfo.Description.Usage |= ImageUsage::Source;

    m_CreateInfo.AdditionalViews.reserve(m_CreateInfo.Description.AdditionalViews.size());
    for (auto& view : m_CreateInfo.Description.AdditionalViews)
        m_CreateInfo.AdditionalViews.push_back(view);
}

Image Image::Create(const Builder::CreateInfo& createInfo)
{
    Image image = {};
    
    switch (createInfo.SourceInfo)
    {
    case CreateInfo::SourceInfo::None:
        image = CreateImage(createInfo);
        break;
    case CreateInfo::SourceInfo::Asset:
        image = CreateImageFromAsset(createInfo);
        break;
    case CreateInfo::SourceInfo::Equirectangular:
        image = CreateImageFromEquirectangular(createInfo);
        break;
    case CreateInfo::SourceInfo::Pixels:
        image = CreateImageFromPixelData(createInfo);
        break;
    default:
        ASSERT(false, "Unrecognized image source info")
        std::unreachable();
    }
    
    return image;
}

void Image::Destroy(const Image& image)
{
    Device::Destroy(image.Handle());
}

ImageSubresource Image::Subresource() const
{
    ImageSubresource imageSubresource = {
        .Image = this,
        .Description = {
            .MipmapBase = 0,
            .Mipmaps = (i8)m_Description.Mipmaps,
            .LayerBase = 0,
            .Layers = (i8)ImageDescription::GetLayers(m_Description)}};
    
    return imageSubresource;
}

ImageSubresource Image::Subresource(i8 mipCount, i8 layerCount) const
{
    return Subresource(0, mipCount, 0, layerCount);
}

ImageSubresource Image::Subresource(u8 mipBase, i8 mipCount, u8 layerBase, i8 layerCount) const
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

ImageSubresource Image::Subresource(const ImageSubresourceDescription& description) const
{
    return ImageSubresource{
        .Image = this,
        .Description = description};
}

ImageBlitInfo Image::BlitInfo() const
{
    return BlitInfo(glm::uvec3{}, glm::uvec3{
        m_Description.Width,
        m_Description.Height,
        ImageDescription::GetDepth(m_Description)},
        0, 0, ImageDescription::GetLayers(m_Description));
}

ImageBlitInfo Image::BlitInfo(u32 mipBase, u32 layerBase, u32 layerCount) const
{
    return BlitInfo(glm::uvec3{}, glm::uvec3{
        m_Description.Width,
        m_Description.Height,
        ImageDescription::GetDepth(m_Description)},
        mipBase, layerBase, layerCount);
}

ImageBlitInfo Image::BlitInfo(const glm::uvec3& bottom, const glm::uvec3& top, u32 mipBase, u32 layerBase,
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

ImageBlitInfo Image::CopyInfo() const
{
    return BlitInfo();
}

ImageBlitInfo Image::CopyInfo(u32 mipBase, u32 layerBase, u32 layerCount) const
{
    return BlitInfo(mipBase, layerBase, layerCount);
}

ImageBlitInfo Image::CopyInfo(const glm::uvec3& bottom, const glm::uvec3& size, u32 mipBase, u32 layerBase,
    u32 layerCount) const
{
    return BlitInfo(bottom, size, mipBase, layerBase, layerCount);
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
    if (subresource == ImageSubresourceDescription{} || subresource == Subresource().Description)
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

void Image::CreateMipmaps(const CommandBuffer& cmd, ImageLayout currentLayout) const
{
    if (m_Description.Mipmaps == 1)
        return;

    i32 width = (i32)m_Description.Width;
    i32 height = (i32)m_Description.Height;
    i32 depth = (i32)ImageDescription::GetDepth(m_Description);
    i8 layers = ImageDescription::GetLayers(m_Description);
    
    ImageSubresource imageSubresource = Subresource(0, 1, 0, layers);
    PrepareForMipmapSource(cmd, imageSubresource, currentLayout);
    for (i8 mip = 1; mip < m_Description.Mipmaps; mip++)
    {
        ImageBlitInfo source = BlitInfo({}, {
            width, height, depth},
            mip - 1, 0, layers);

        width = std::max(1, width >> 1);
        height = std::max(1, height >> 1);
        depth = std::max(1, depth >> 1);

        ImageBlitInfo destination = BlitInfo({}, {
            width, height, depth},
            mip, 0, layers);

        ImageSubresource mipmapSubresource = Subresource(mip, 1, 0, layers);
        PrepareForMipmapDestination(cmd, mipmapSubresource);
        RenderCommand::BlitImage(cmd, source, destination, m_Description.MipmapFilter);
        PrepareForMipmapSource(cmd, mipmapSubresource, ImageLayout::Destination);
    }
}

glm::uvec3 Image::GetPixelCoordinate(const glm::vec3& coordinate, ImageSizeType sizeType) const
{
    if (sizeType == ImageSizeType::Absolute)
        return glm::uvec3{coordinate};

    glm::uvec3 size = {
        m_Description.Width,
        m_Description.Height,
        ImageDescription::GetDepth(m_Description)};

    return glm::uvec3 {
        (u32)((f32)size.x * coordinate.x), (u32)((f32)size.y * coordinate.y), (u32)((f32)size.z * coordinate.z)};
}

Image Image::CreateImage(const CreateInfo& createInfo)
{
    Image image = AllocateImage(createInfo);
    
    CreateImageView(image.Subresource(), createInfo.AdditionalViews);

    return image;
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

Image Image::CreateImageFromEquirectangular(const CreateInfo& createInfo)
{
    Image image = CreateImage(createInfo);

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
    CreateImageView(image.Subresource(), createInfo.AdditionalViews);
    
    ImageSubresource imageSubresource = image.Subresource(0, 1, 0, 1);

    Device::ImmediateSubmit([&](const CommandBuffer& cmd)
    {
        PrepareForMipmapDestination(cmd, imageSubresource);
        CopyBufferToImage(cmd, createInfo.DataBuffer, image);
        if (!createInfo.NoMips)
            image.CreateMipmaps(cmd, ImageLayout::Destination);
        imageSubresource.Description.Mipmaps = createInfo.Description.Mipmaps;
    });
    Device::Destroy(createInfo.DataBuffer.Handle());
    
    return image;
}

Image Image::AllocateImage(const CreateInfo& createInfo)
{
    return Device::AllocateImage(createInfo);
}

void Image::PrepareForMipmapDestination(const CommandBuffer& cmd, const ImageSubresource& imageSubresource)
{
    PrepareImageGeneral(cmd, imageSubresource,
        ImageLayout::Undefined, ImageLayout::Destination,
        PipelineAccess::None, PipelineAccess::WriteTransfer,
        PipelineStage::AllTransfer, PipelineStage::AllTransfer);
}

void Image::PrepareForMipmapSource(const CommandBuffer& cmd, const ImageSubresource& imageSubresource,
    ImageLayout currentLayout)
{
    PrepareImageGeneral(cmd, imageSubresource,
        currentLayout, ImageLayout::Source,
        PipelineAccess::WriteAll, PipelineAccess::ReadTransfer,
        PipelineStage::AllCommands, PipelineStage::AllTransfer);
}

void Image::PrepareImageGeneral(const CommandBuffer& cmd,
    const ImageSubresource& imageSubresource,
    ImageLayout current, ImageLayout target,
    PipelineAccess srcAccess, PipelineAccess dstAccess,
    PipelineStage srcStage, PipelineStage dstStage)
{
    DeletionQueue deletionQueue = {};
    
    DependencyInfo layoutTransition = Device::CreateDependencyInfo({
        .LayoutTransitionInfo = LayoutTransitionInfo{
            .ImageSubresource = imageSubresource,
            .SourceStage = srcStage,
            .DestinationStage = dstStage,
            .SourceAccess = srcAccess,
            .DestinationAccess = dstAccess,
            .OldLayout = current,
            .NewLayout = target}});
    deletionQueue.Enqueue(layoutTransition);
    
    RenderCommand::WaitOnBarrier(cmd, layoutTransition);
}

void Image::CopyBufferToImage(const CommandBuffer& cmd, const Buffer& buffer, const Image& image)
{
    RenderCommand::CopyBufferToImage(cmd, buffer,
        image.Subresource(0, 1, 0, ImageDescription::GetLayers(image.m_Description)));
}

void Image::CreateImageView(const ImageSubresource& imageSubresource,
    const std::vector<ImageSubresourceDescription>& additionalViews)
{
    ASSERT(imageSubresource.Description.MipmapBase + imageSubresource.Description.Mipmaps <=
        imageSubresource.Image->m_Description.Mipmaps,
        "Incorrect mipmap range for image view")
    ASSERT(imageSubresource.Description.LayerBase + imageSubresource.Description.Layers <=
        ImageDescription::GetLayers(imageSubresource.Image->m_Description),
        "Incorrect layer range for image view")

    Device::CreateViews(imageSubresource, additionalViews);
}


