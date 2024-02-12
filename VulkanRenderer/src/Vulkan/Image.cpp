#include "Image.h"

#include "Core/core.h"
#include "Driver.h"
#include "VulkanUtils.h"

#include "Buffer.h"
#include "RenderCommand.h"
#include "AssetLib.h"
#include "AssetManager.h"
#include "TextureAsset.h"

namespace
{
    static_assert(ImageDescription::ALL_MIPMAPS == VK_REMAINING_MIP_LEVELS, "Incorrect value for `ALL_MIPMAPS`");
    static_assert(ImageDescription::ALL_LAYERS == VK_REMAINING_ARRAY_LAYERS, "Incorrect value for `ALL_LAYERS`");
    static_assert(Sampler::LOD_MAX == VK_LOD_CLAMP_NONE, "Incorrect value for `LOD_MAX`");
    
    constexpr VkFormat vulkanFormatFromImageFormat(ImageFormat format)
    {
        switch (format)
        {
        case ImageFormat::Undefined:            return VK_FORMAT_UNDEFINED;
        case ImageFormat::R8_UNORM:             return VK_FORMAT_R8_UNORM; 
        case ImageFormat::R8_SNORM:             return VK_FORMAT_R8_SNORM;
        case ImageFormat::R8_UINT:              return VK_FORMAT_R8_UINT;
        case ImageFormat::R8_SINT:              return VK_FORMAT_R8_SINT;
        case ImageFormat::R8_SRGB:              return VK_FORMAT_R8_SRGB;
        case ImageFormat::RG8_UNORM:            return VK_FORMAT_R8G8_UNORM;
        case ImageFormat::RG8_SNORM:            return VK_FORMAT_R8G8_SNORM;
        case ImageFormat::RG8_UINT:             return VK_FORMAT_R8G8_UINT;
        case ImageFormat::RG8_SINT:             return VK_FORMAT_R8G8_SINT;
        case ImageFormat::RG8_SRGB:             return VK_FORMAT_R8G8_SRGB;
        case ImageFormat::RGBA8_UNORM:          return VK_FORMAT_R8G8B8A8_UNORM;
        case ImageFormat::RGBA8_SNORM:          return VK_FORMAT_R8G8B8A8_SNORM;
        case ImageFormat::RGBA8_UINT:           return VK_FORMAT_R8G8B8A8_UINT;
        case ImageFormat::RGBA8_SINT:           return VK_FORMAT_R8G8B8A8_SINT;
        case ImageFormat::RGBA8_SRGB:           return VK_FORMAT_R8G8B8A8_SRGB;
        case ImageFormat::R16_UNORM:            return VK_FORMAT_R16_UNORM;
        case ImageFormat::R16_SNORM:            return VK_FORMAT_R16_SNORM;
        case ImageFormat::R16_UINT:             return VK_FORMAT_R16_UINT;
        case ImageFormat::R16_SINT:             return VK_FORMAT_R16_SINT;
        case ImageFormat::R16_FLOAT:            return VK_FORMAT_R16_SFLOAT;
        case ImageFormat::RG16_UNORM:           return VK_FORMAT_R16G16_UNORM;
        case ImageFormat::RG16_SNORM:           return VK_FORMAT_R16G16_SNORM;
        case ImageFormat::RG16_UINT:            return VK_FORMAT_R16G16_UINT;
        case ImageFormat::RG16_SINT:            return VK_FORMAT_R16G16_SINT;
        case ImageFormat::RG16_FLOAT:           return VK_FORMAT_R16G16_SFLOAT;
        case ImageFormat::RGBA16_UNORM:         return VK_FORMAT_R16G16B16A16_UNORM;
        case ImageFormat::RGBA16_SNORM:         return VK_FORMAT_R16G16B16A16_SNORM;
        case ImageFormat::RGBA16_UINT:          return VK_FORMAT_R16G16B16A16_UINT;
        case ImageFormat::RGBA16_SINT:          return VK_FORMAT_R16G16B16A16_SINT;
        case ImageFormat::RGBA16_FLOAT:         return VK_FORMAT_R16G16B16A16_SFLOAT;
        case ImageFormat::R32_UINT:             return VK_FORMAT_R32_UINT;
        case ImageFormat::R32_SINT:             return VK_FORMAT_R32_SINT;
        case ImageFormat::R32_FLOAT:            return VK_FORMAT_R32_SFLOAT;
        case ImageFormat::RG32_UINT:            return VK_FORMAT_R32G32_UINT;
        case ImageFormat::RG32_SINT:            return VK_FORMAT_R32G32_SINT;
        case ImageFormat::RG32_FLOAT:           return VK_FORMAT_R32G32_SFLOAT;
        case ImageFormat::RGBA32_UINT:          return VK_FORMAT_R32G32_UINT;
        case ImageFormat::RGBA32_SINT:          return VK_FORMAT_R32G32_SINT;
        case ImageFormat::RGBA32_FLOAT:         return VK_FORMAT_R32G32_SFLOAT;
        case ImageFormat::RGB10A2:              return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
        case ImageFormat::R11G11B10:            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case ImageFormat::D32_FLOAT:            return VK_FORMAT_D32_SFLOAT;
        case ImageFormat::D24_UNORM_S8_UINT:    return VK_FORMAT_D24_UNORM_S8_UINT;
        case ImageFormat::D32_FLOAT_S8_UINT:    return VK_FORMAT_D32_SFLOAT_S8_UINT;
            
        case ImageFormat::MaxVal:
        default:
            ASSERT(false, "Unsupported image format")
            break;
        }
        std::unreachable();
    }

    constexpr VkImageLayout vulkanImageLayoutFromImageLayout(ImageLayout layout)
    {
        switch (layout)
        {
        case ImageLayout::Undefined:                return VK_IMAGE_LAYOUT_UNDEFINED;
        case ImageLayout::General:                  return VK_IMAGE_LAYOUT_GENERAL;
        case ImageLayout::Attachment:               return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        case ImageLayout::ReadOnly:                 return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        case ImageLayout::ColorAttachment:          return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ImageLayout::Present:                  return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case ImageLayout::DepthStencilAttachment:   return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilReadonly:     return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ImageLayout::DepthAttachment:          return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthReadonly:            return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
        case ImageLayout::Source:                   return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ImageLayout::Destination:              return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        default:
            ASSERT(false, "Unsupported image format")
            break;
        }
        std::unreachable();
    }

    constexpr ImageFormat imageFormatFromAssetFormat(assetLib::TextureFormat format)
    {
        switch (format)
        {
        case assetLib::TextureFormat::Unknown:
            return ImageFormat::Undefined;
        case assetLib::TextureFormat::SRGBA8:
            return ImageFormat::RGBA8_SRGB;
        case assetLib::TextureFormat::RGBA8:
            return ImageFormat::RGBA8_UNORM;
        default:
            ASSERT(false, "Unsupported image format")
            break;
        }
        std::unreachable();
    }

    constexpr VkImageUsageFlags vulkanImageUsageFromImageUsage(ImageUsage usage)
    {
        ASSERT(!enumHasAll(usage, ImageUsage::Color | ImageUsage::Depth | ImageUsage::Stencil),
            "Image usage cannot include both color and depth/stencil")

        std::vector<std::pair<ImageUsage, VkImageUsageFlags>> MAPPINGS {
            {ImageUsage::Sampled,       VK_IMAGE_USAGE_SAMPLED_BIT},
            {ImageUsage::Color,         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
            {ImageUsage::Depth,         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
            {ImageUsage::Stencil,       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
            {ImageUsage::Storage,       VK_IMAGE_USAGE_STORAGE_BIT},
            {ImageUsage::Source,        VK_IMAGE_USAGE_TRANSFER_SRC_BIT},
            {ImageUsage::Destination,   VK_IMAGE_USAGE_TRANSFER_DST_BIT}
        };
        
        VkImageUsageFlags flags = 0;
        for (auto&& [iu, vulkanIu] : MAPPINGS)
            if (enumHasAny(usage, iu))
                flags |= vulkanIu;

        return flags;
    }

    constexpr VkImageAspectFlags vulkanImageAspectFromImageUsage(ImageUsage usage)
    {
        if (enumHasAny(usage, ImageUsage::Depth))
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        if (enumHasAny(usage, ImageUsage::Stencil))
            return VK_IMAGE_ASPECT_STENCIL_BIT;

        // todo: this is probably incorrect
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    constexpr VkImageType vulkanImageTypeFromImageKind(ImageKind kind)
    {
        switch (kind)
        {
        case ImageKind::Image2d:
            return VK_IMAGE_TYPE_2D;
        case ImageKind::Image3d:
            return VK_IMAGE_TYPE_3D;
        default:
            ASSERT(false, "Unsupported image kind")
            break;
        }
        std::unreachable();
    }
    
    constexpr VkImageViewType vulkanImageViewTypeFromImageKind(ImageKind kind)
    {
        switch (kind)
        {
        case ImageKind::Image2d:
            return VK_IMAGE_VIEW_TYPE_2D;
        case ImageKind::Image3d:
            return VK_IMAGE_VIEW_TYPE_3D;
        default:
            ASSERT(false, "Unsupported image kind")
            break;
        }
        std::unreachable();
    }
    
    constexpr VkFilter vulkanFilterFromImageFilter(ImageFilter filter)
    {
        switch (filter)
        {
        case ImageFilter::Linear:
            return VK_FILTER_LINEAR;
        case ImageFilter::Nearest:
            return VK_FILTER_NEAREST;
        default:
            ASSERT(false, "Unsupported filter format")
        }
        std::unreachable();
    }
    
    constexpr VkSamplerMipmapMode vulkanMipmapModeFromSamplerFilter(VkFilter filter)
    {
        switch (filter)
        {
        case VK_FILTER_NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case VK_FILTER_LINEAR:
        case VK_FILTER_CUBIC_IMG:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:
            ASSERT(false, "Unsupported filter format")
        }
        std::unreachable();
    }

    constexpr VkSamplerReductionMode vulkanSamplerReductionModeFromSamplerReductionMode(SamplerReductionMode mode)
    {
        switch (mode)
        {
        case SamplerReductionMode::Average:
            return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
        case SamplerReductionMode::Min:
            return VK_SAMPLER_REDUCTION_MODE_MIN;
        case SamplerReductionMode::Max:
            return VK_SAMPLER_REDUCTION_MODE_MAX;
        default:
            ASSERT(false, "Unsupported sampler reduction mode")
        }
        std::unreachable();
    }

    constexpr VkSamplerAddressMode vulkanSamplerAddressModeFromSamplerWrapMode(SamplerWrapMode mode)
    {
        switch (mode)
        {
        case SamplerWrapMode::ClampEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerWrapMode::ClampBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case SamplerWrapMode::Repeat:
            return  VK_SAMPLER_ADDRESS_MODE_REPEAT;
        default:
            ASSERT(false, "Unsupported sampler wrap mode")
        }
        std::unreachable();
    }
}

Sampler Sampler::Builder::Build()
{
    return SamplerCache::CreateSampler(m_CreateInfo);
}

Sampler::Builder& Sampler::Builder::Filters(ImageFilter minification, ImageFilter magnification)
{
    m_CreateInfo.MinificationFilter = vulkanFilterFromImageFilter(minification);
    m_CreateInfo.MagnificationFilter = vulkanFilterFromImageFilter(magnification);
    m_CreateInfo.MipmapFilter = vulkanMipmapModeFromSamplerFilter(m_CreateInfo.MinificationFilter);

    return *this;
}

Sampler::Builder& Sampler::Builder::WrapMode(SamplerWrapMode mode)
{
    m_CreateInfo.AddressMode = vulkanSamplerAddressModeFromSamplerWrapMode(mode);

    return *this;
}

Sampler::Builder& Sampler::Builder::ReductionMode(SamplerReductionMode reductionMode)
{
    m_CreateInfo.ReductionMode = vulkanSamplerReductionModeFromSamplerReductionMode(reductionMode);

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

void Sampler::Destroy(const Sampler& sampler)
{
    vkDestroySampler(Driver::DeviceHandle(), sampler.m_Sampler, nullptr);
}

Image::Builder::Builder(const ImageDescription& description)
{
    m_CreateInfo.Description = description;
    m_CreateInfo.Format = vulkanFormatFromImageFormat(description.Format);
    m_CreateInfo.ImageAspect = vulkanImageAspectFromImageUsage(description.Usage);
    m_CreateInfo.ImageUsage = vulkanImageUsageFromImageUsage(description.Usage);
}

Image Image::Builder::Build()
{
    PreBuild();
    
    Image image = Image::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([image](){ Image::Destroy(image); });

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
                    
        m_CreateInfo.Description.Format = imageFormatFromAssetFormat(textureInfo.Format);
        m_CreateInfo.Description.Width = textureInfo.Dimensions.Width;
        m_CreateInfo.Description.Height = textureInfo.Dimensions.Height;
        // todo: not always correct, should reflect in in asset file
        m_CreateInfo.Description.Kind = ImageKind::Image2d;
        m_CreateInfo.Description.Layers = 1;
        m_CreateInfo.Format = vulkanFormatFromImageFormat(m_CreateInfo.Description.Format);
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

Image::Builder& Image::Builder::SetFormat(ImageFormat format)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset, "Cannot use custom format when loading from file")
    
    m_CreateInfo.Description.Format = format;
    m_CreateInfo.Format = vulkanFormatFromImageFormat(format);
    
    return *this;
}

Image::Builder& Image::Builder::SetExtent(const glm::uvec2& extent)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset, "Cannot set extent when loading from file")

    m_CreateInfo.Description.Width = extent.x;
    m_CreateInfo.Description.Height = extent.y;
    
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

Image::Builder& Image::Builder::CreateMipmaps(bool enable, ImageFilter filter)
{
    m_CreateMipmaps = enable;
    m_CreateInfo.MipmapFilter = vulkanFilterFromImageFilter(filter);
    
    return *this;
}

Image::Builder& Image::Builder::SetUsage(ImageUsage usage)
{
    m_CreateInfo.Description.Usage = usage;

    return *this;
}

void Image::Builder::PreBuild()
{
    if (m_CreateInfo.SourceInfo == CreateInfo::SourceInfo::Asset)
       m_CreateInfo.Description.Usage |= ImageUsage::Destination;
    
    if (m_CreateMipmaps)
    {
        m_CreateInfo.Description.Usage |= ImageUsage::Destination | ImageUsage::Source;
        u32 maxDimension = std::max(m_CreateInfo.Description.Width, m_CreateInfo.Description.Height);
        if (m_CreateInfo.Description.Kind == ImageKind::Image3d)
            maxDimension = std::max(maxDimension, m_CreateInfo.Description.Layers);
        m_CreateInfo.Description.Mipmaps = (u32)std::log2(maxDimension) + 1;    
    }
    
    if (enumHasAny(m_CreateInfo.Description.Usage, ImageUsage::Readback))
        m_CreateInfo.Description.Usage |= ImageUsage::Source;
    
    m_CreateInfo.ImageUsage = vulkanImageUsageFromImageUsage(m_CreateInfo.Description.Usage);
    m_CreateInfo.ImageAspect = vulkanImageAspectFromImageUsage(m_CreateInfo.Description.Usage);
}

Image Image::Create(const Builder::CreateInfo& createInfo)
{
    Image image = {};
    
    switch (createInfo.SourceInfo)
    {
    case CreateInfo::SourceInfo::None:
    {
        image = AllocateImage(createInfo);
        image.m_View = Image::CreateVulkanImageView(image.CreateSubresource());
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
    if (image.m_Allocation != VK_NULL_HANDLE)
    {
        vkDestroyImageView(Driver::DeviceHandle(), image.m_View, nullptr);
        vmaDestroyImage(Driver::Allocator(), image.m_Image, image.m_Allocation);
    }
}

ImageSubresource Image::CreateSubresource() const
{
    ImageSubresource imageSubresource = {
        .Image = this,
        .MipmapBase = 0,
        .Mipmaps = m_Description.Mipmaps,
        .LayerBase = 0,
        .Layers = m_Description.Layers};
    
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
        .MipmapBase = mipBase,
        .Mipmaps = mipCount,
        .LayerBase = layerBase,
        .Layers = layerCount};

    return imageSubresource;
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

ImageBindingInfo Image::CreateBindingInfo(ImageFilter filter, ImageLayout layout,
    const ImageViewList& views, ImageViewHandle handle) const
{
    return CreateBindingInfo(Sampler::Builder().Filters(filter, filter).Build(), layout, views, handle);
}

ImageBindingInfo Image::CreateBindingInfo(Sampler sampler, ImageLayout layout,
    const ImageViewList& views, ImageViewHandle handle) const
{
    return ImageBindingInfo {
        .Image = this,
        .Sampler = sampler,
        .Layout = layout,
        .ViewList = &views,
        .ViewHandle = handle};
}

Image Image::CreateImageFromAsset(const CreateInfo& createInfo)
{
    Image image = {};
    
    if (createInfo.AssetInfo.Status == CreateInfo::AssetInfo::AssetStatus::Reused)
    {
        image = *AssetManager::GetImage(createInfo.AssetInfo.AssetPath);
        image.m_Allocation = VK_NULL_HANDLE; // to avoid multiple deletions of the same texture
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
    imageSubresource.Mipmaps = createInfo.Description.Mipmaps;
    CreateMipmaps(image, createInfo);
    PrepareForShaderRead(imageSubresource);
    
    image.m_View = CreateVulkanImageView(image.CreateSubresource());
        
    return image;
}

Image Image::AllocateImage(const CreateInfo& createInfo)
{
    Image image = {};
    
    image.m_Description = createInfo.Description;

    u32 depth = image.m_Description.Kind != ImageKind::Image3d ? 1u : image.m_Description.Layers;
    u32 layers = image.m_Description.Kind != ImageKind::Image3d ? image.m_Description.Layers : 1u;
    
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.format = createInfo.Format;
    imageCreateInfo.usage = createInfo.ImageUsage;
    imageCreateInfo.extent = {
        .width = image.m_Description.Width,
        .height = image.m_Description.Height,
        .depth = depth};
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.imageType = vulkanImageTypeFromImageKind(createInfo.Description.Kind);
    imageCreateInfo.mipLevels = createInfo.Description.Mipmaps;
    imageCreateInfo.arrayLayers = layers;

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VulkanCheck(vmaCreateImage(Driver::Allocator(), &imageCreateInfo, &allocationInfo, &image.m_Image, &image.m_Allocation, nullptr),
        "Failed to create image");

    return image;
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
    ImageLayout current = imageSubresource.Mipmaps > 1 ? ImageLayout::Source : ImageLayout::Destination;
    PrepareImageGeneral(imageSubresource,
       current, ImageLayout::ReadOnly,
       PipelineAccess::ReadTransfer, PipelineAccess::ReadShader,
       PipelineStage::AllTransfer, PipelineStage::FragmentShader);
}

void Image::PrepareImageGeneral(const ImageSubresource& imageSubresource,
    ImageLayout current, ImageLayout target,
    PipelineAccess srcAccess, PipelineAccess dstAccess,
    PipelineStage srcStage, PipelineStage dstStage)
{
    DependencyInfo layoutTransition = DependencyInfo::Builder()
        .LayoutTransition({
            .ImageSubresource = &imageSubresource,
            .SourceStage = srcStage,
            .DestinationStage = dstStage,
            .SourceAccess = srcAccess,
            .DestinationAccess = dstAccess,
            .OldLayout = current,
            .NewLayout = target})
        .Build();
    
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
    imageSubresource.Layers = layerCount;
    imageSubresource.Mipmaps = 1;
    
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

VkImageView Image::CreateVulkanImageView(const ImageSubresource& imageSubresource)
{
    return CreateVulkanImageView(imageSubresource,
        vulkanFormatFromImageFormat(imageSubresource.Image->m_Description.Format));
}

VkImageView Image::CreateVulkanImageView(const ImageSubresource& imageSubresource, VkFormat format)
{
    ASSERT(imageSubresource.MipmapBase + imageSubresource.Mipmaps <= imageSubresource.Image->m_Description.Mipmaps,
        "Incorrect mipmap range for image view")
    ASSERT(imageSubresource.LayerBase + imageSubresource.Layers <= imageSubresource.Image->m_Description.Layers,
        "Incorrect layer range for image view")
    
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = imageSubresource.Image->m_Image;
    createInfo.format = format;
    createInfo.viewType = vulkanImageViewTypeFromImageKind(imageSubresource.Image->m_Description.Kind);
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = vulkanImageAspectFromImageUsage(
        imageSubresource.Image->m_Description.Usage);
    createInfo.subresourceRange.baseMipLevel = imageSubresource.MipmapBase;
    createInfo.subresourceRange.levelCount = imageSubresource.Mipmaps;
    createInfo.subresourceRange.baseArrayLayer = imageSubresource.LayerBase;
    createInfo.subresourceRange.layerCount = imageSubresource.Layers;

    VkImageView imageView;

    VulkanCheck(vkCreateImageView(Driver::DeviceHandle(), &createInfo, nullptr, &imageView),
        "Failed to create image view");

    return imageView;
}

void Image::FillVulkanLayoutTransitionBarrier(const LayoutTransitionInfo& layoutTransitionInfo,
    VkImageMemoryBarrier2& barrier)
{
    barrier.oldLayout = vulkanImageLayoutFromImageLayout(layoutTransitionInfo.OldLayout);
    barrier.newLayout = vulkanImageLayoutFromImageLayout(layoutTransitionInfo.NewLayout);
    barrier.image = layoutTransitionInfo.ImageSubresource->Image->m_Image;
    barrier.subresourceRange = {
        .aspectMask = vulkanImageAspectFromImageUsage(
            layoutTransitionInfo.ImageSubresource->Image->m_Description.Usage),
        .baseMipLevel = layoutTransitionInfo.ImageSubresource->MipmapBase,
        .levelCount = layoutTransitionInfo.ImageSubresource->Mipmaps,
        .baseArrayLayer = layoutTransitionInfo.ImageSubresource->LayerBase,
        .layerCount = layoutTransitionInfo.ImageSubresource->Layers};
}

std::pair<VkBlitImageInfo2, VkImageBlit2> Image::CreateVulkanBlitInfo(
    const ImageBlitInfo& source, const ImageBlitInfo& destination, ImageFilter filter)
{
    VkImageBlit2 imageBlit = {};
    VkBlitImageInfo2 blitImageInfo = {};
    
    imageBlit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    imageBlit.srcSubresource.aspectMask = vulkanImageAspectFromImageUsage(source.Image->m_Description.Usage);
    imageBlit.srcSubresource.baseArrayLayer = source.LayerBase;
    imageBlit.srcSubresource.layerCount = source.Layers;
    imageBlit.srcSubresource.mipLevel = source.MipmapBase;
    imageBlit.srcOffsets[0] = VkOffset3D{
        .x = (i32)source.Bottom.x,
        .y = (i32)source.Bottom.y,
        .z = (i32)source.Bottom.z};
    imageBlit.srcOffsets[1] = VkOffset3D{
        .x = (i32)source.Top.x,
        .y = (i32)source.Top.y,
        .z = (i32)source.Top.z};

    imageBlit.dstSubresource.aspectMask = vulkanImageAspectFromImageUsage(destination.Image->m_Description.Usage);
    imageBlit.dstSubresource.baseArrayLayer = destination.LayerBase;
    imageBlit.dstSubresource.layerCount = destination.Layers;
    imageBlit.dstSubresource.mipLevel = destination.MipmapBase;
    imageBlit.dstOffsets[0] = VkOffset3D{
        .x = (i32)destination.Bottom.x,
        .y = (i32)destination.Bottom.y,
        .z = (i32)destination.Bottom.z};
    imageBlit.dstOffsets[1] = VkOffset3D{
        .x = (i32)destination.Top.x,
        .y = (i32)destination.Top.y,
        .z = (i32)destination.Top.z};
    
    blitImageInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blitImageInfo.srcImage = source.Image->m_Image;
    blitImageInfo.dstImage = destination.Image->m_Image;
    blitImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitImageInfo.regionCount = 1;
    blitImageInfo.filter = vulkanFilterFromImageFilter(filter);

    return {blitImageInfo, imageBlit};
}

VkBufferImageCopy2 Image::CreateVulkanImageCopyInfo(const ImageSubresource& subresource)
{
    ASSERT(subresource.Mipmaps == 1, "Buffer to image copies one mipmap at a time")
    
    VkBufferImageCopy2 bufferImageCopy = {};
    bufferImageCopy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
    bufferImageCopy.imageExtent = {
        .width = subresource.Image->m_Description.Width,
        .height = subresource.Image->m_Description.Height,
        .depth = subresource.Image->m_Description.Kind == ImageKind::Image3d ?
            (i32)subresource.Image->m_Description.Layers : 1u};
    bufferImageCopy.imageSubresource.aspectMask = vulkanImageAspectFromImageUsage(
        subresource.Image->m_Description.Usage);
    bufferImageCopy.imageSubresource.mipLevel = subresource.MipmapBase;
    bufferImageCopy.imageSubresource.baseArrayLayer = subresource.LayerBase;
    bufferImageCopy.imageSubresource.layerCount = subresource.Layers;

    return bufferImageCopy;
}

VkDescriptorImageInfo Image::CreateVulkanImageDescriptor(const ImageBindingInfo& imageBindingInfo)
{
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = imageBindingInfo.Sampler.m_Sampler;
    descriptorTextureInfo.imageView = imageBindingInfo.ViewList == nullptr ?
        imageBindingInfo.Image->m_View :
        (*imageBindingInfo.ViewList)[imageBindingInfo.ViewHandle];
    descriptorTextureInfo.imageLayout = vulkanImageLayoutFromImageLayout(imageBindingInfo.Layout);

    return descriptorTextureInfo;
}

VkRenderingAttachmentInfo Image::CreateVulkanRenderingAttachment(const Image& image, ImageLayout layout)
{
    VkRenderingAttachmentInfo renderingAttachmentInfo = {};
    renderingAttachmentInfo.imageView = image.m_View;
    renderingAttachmentInfo.imageLayout = vulkanImageLayoutFromImageLayout(layout);

    return renderingAttachmentInfo;
}

VkPipelineRenderingCreateInfo Image::CreateVulkanRenderingInfo(const RenderingDetails& renderingDetails,
    std::vector<VkFormat>& colorFormats)
{
    for (u32 colorIndex = 0; colorIndex < renderingDetails.ColorFormats.size(); colorIndex++)
        colorFormats[colorIndex] = vulkanFormatFromImageFormat(renderingDetails.ColorFormats[colorIndex]);

    VkPipelineRenderingCreateInfo renderingCreateInfo = {};
    renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.colorAttachmentCount = (u32)renderingDetails.ColorFormats.size();
    renderingCreateInfo.pColorAttachmentFormats = colorFormats.data();
    renderingCreateInfo.depthAttachmentFormat = vulkanFormatFromImageFormat(renderingDetails.DepthFormat);

    return renderingCreateInfo;
}

ImageViewList ImageViewList::Builder::Build()
{
    ImageViewList list = ImageViewList::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([list]() { ImageViewList::Destroy(list); });

    return list;
}

ImageViewList ImageViewList::Builder::BuildManualLifetime()
{
    return ImageViewList::Create(m_CreateInfo);
}

ImageViewList::Builder& ImageViewList::Builder::ForImage(const Image& image)
{
    m_CreateInfo.Image = &image;

    return *this;
}

ImageViewList::Builder& ImageViewList::Builder::Add(const ImageSubresource& subresource, ImageViewHandle& handle)
{
    ASSERT(m_CreateInfo.Image == nullptr || m_CreateInfo.Image == subresource.Image, "Foreign subresource")
    
    VkImageView view = Image::CreateVulkanImageView(subresource);
    handle.m_Index = (u32)m_CreateInfo.ImageViews.size();
    m_CreateInfo.ImageViews.push_back(view);

    return *this;
}

ImageViewList ImageViewList::Create(const Builder::CreateInfo& createInfo)
{
    ImageViewList list = {};
    list.m_Image = createInfo.Image;
    list.m_Views = createInfo.ImageViews;

    return list;
}

void ImageViewList::Destroy(const ImageViewList& imageViews)
{
    for (auto& view : imageViews.m_Views)
        vkDestroyImageView(Driver::DeviceHandle(), view, nullptr);
}

Sampler SamplerCache::CreateSampler(const Sampler::Builder::CreateInfo& createInfo)
{
    CacheKey key = {.CreateInfo = createInfo};

    if (s_SamplerCache.contains(key))
        return s_SamplerCache.at(key);

    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = createInfo.MagnificationFilter;
    samplerCreateInfo.minFilter = createInfo.MinificationFilter;
    samplerCreateInfo.mipmapMode = createInfo.MipmapFilter;
    samplerCreateInfo.addressModeU = createInfo.AddressMode;
    samplerCreateInfo.addressModeV = createInfo.AddressMode;
    samplerCreateInfo.addressModeW = createInfo.AddressMode;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = createInfo.MaxLod;
    samplerCreateInfo.maxAnisotropy = Driver::GetAnisotropyLevel();
    samplerCreateInfo.anisotropyEnable = (u32)createInfo.WithAnisotropy;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSamplerReductionModeCreateInfo reductionModeCreateInfo = {};
    if (createInfo.ReductionMode.has_value())
    {
        reductionModeCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
        reductionModeCreateInfo.reductionMode = *createInfo.ReductionMode;
        samplerCreateInfo.pNext = &reductionModeCreateInfo;    
    }

    VkSampler sampler;
    
    VulkanCheck(vkCreateSampler(Driver::DeviceHandle(), &samplerCreateInfo, nullptr, &sampler),
        "Failed to create depth pyramid sampler");


    Sampler newSampler = {};
    newSampler.m_Sampler = sampler;
    s_SamplerCache.emplace(key, newSampler);
    
    Driver::DeletionQueue().AddDeleter([newSampler](){ Sampler::Destroy(newSampler); });

    return newSampler;
}

bool SamplerCache::CacheKey::operator==(const CacheKey& other) const
{
    return
        CreateInfo.MinificationFilter == other.CreateInfo.MinificationFilter &&
        CreateInfo.MagnificationFilter == other.CreateInfo.MagnificationFilter &&
        CreateInfo.MipmapFilter == other.CreateInfo.MipmapFilter &&
        CreateInfo.ReductionMode == other.CreateInfo.ReductionMode &&
        CreateInfo.MaxLod == other.CreateInfo.MaxLod &&
        CreateInfo.WithAnisotropy == other.CreateInfo.WithAnisotropy;
}

u64 SamplerCache::SamplerKeyHash::operator()(const CacheKey& cacheKey) const
{
    u64 hashKey =
        cacheKey.CreateInfo.MagnificationFilter |
        (cacheKey.CreateInfo.MagnificationFilter << 1) |
        (cacheKey.CreateInfo.MipmapFilter << 2) |
        (cacheKey.CreateInfo.WithAnisotropy << 3) |
        (cacheKey.CreateInfo.ReductionMode.has_value() << 4) |
        ((cacheKey.CreateInfo.ReductionMode.has_value() ? *cacheKey.CreateInfo.ReductionMode : 0) << 5) |
        ((u64)std::bit_cast<u32>(cacheKey.CreateInfo.MaxLod) << 32);
    u64 hash = std::hash<u64>()(hashKey);
    
    return hash;
}

std::unordered_map<SamplerCache::CacheKey, Sampler, SamplerCache::SamplerKeyHash> SamplerCache::s_SamplerCache = {};
