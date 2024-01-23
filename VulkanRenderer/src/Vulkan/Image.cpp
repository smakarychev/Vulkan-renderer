#include "Image.h"

#include "Core/core.h"
#include "Driver.h"
#include "VulkanUtils.h"

#include "Buffer.h"
#include "RenderCommand.h"
#include "AssetLib.h"
#include "AssetManager.h"
#include "TextureAsset.h"

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
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, "`Asset` in `ImageData` options are incompatible")
    
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
    
        m_CreateInfo.AssetInfo.Buffer = Buffer::Builder()
            .SetKind(BufferKind::Source)
            .SetSizeBytes(textureInfo.SizeBytes)
            .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)
            .BuildManualLifetime();
    
        void* destination = m_CreateInfo.AssetInfo.Buffer.Map();
        assetLib::unpackTexture(textureInfo, textureFile.Blob.data(), textureFile.Blob.size(), (u8*)destination);
        m_CreateInfo.AssetInfo.Buffer.Unmap();

        m_CreateInfo.Format = vkUtils::vkFormatByTextureAssetFormat(textureInfo.Format);
        m_CreateInfo.Extent = {textureInfo.Dimensions.Width, textureInfo.Dimensions.Height};
    }

    return *this;
}

Image::Builder& Image::Builder::FromImageData(const ImageData& imageData)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset, "`Asset` in `ImageData` options are incompatible")
    m_CreateInfo.ImageData = imageData;
    m_CreateInfo.SourceInfo = CreateInfo::SourceInfo::ImageData;
    m_CreateInfo.ImageAspect = imageData.Aspect;

    return *this;
}

Image::Builder& Image::Builder::SetFormat(VkFormat format)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, "Images created using `ImageData` option are immutable")
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset || format == VK_FORMAT_R8G8B8A8_SRGB,
        "Cannot use custom format when loading from file")
    m_CreateInfo.Format = format;
    
    return *this;
}

Image::Builder& Image::Builder::SetExtent(VkExtent2D extent)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, "Images created using `ImageData` option are immutable")
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::Asset,
        "Cannot set extent when loading from file")
    m_CreateInfo.Extent = extent;

    return *this;
}

Image::Builder& Image::Builder::CreateMipmaps(bool enable)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, "Images created using `ImageData` option are immutable")
    m_CreateMipmaps = enable;

    return *this;
}

Image::Builder& Image::Builder::CreateView(bool enable)
{
    ASSERT(m_CreateInfo.SourceInfo != CreateInfo::SourceInfo::ImageData, "Images created using `ImageData` option are immutable")
    m_CreateInfo.CreateView = enable;

    return *this;
}

Image::Builder& Image::Builder::SetUsage(VkImageUsageFlags imageUsage, VkImageAspectFlags imageAspect)
{
    m_CreateInfo.ImageUsage = imageUsage;
    m_CreateInfo.ImageAspect = imageAspect;

    return *this;
}

void Image::Builder::PreBuild()
{
    if (m_CreateInfo.SourceInfo == CreateInfo::SourceInfo::Asset)
        m_CreateInfo.ImageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (m_CreateMipmaps)
    {
        m_CreateInfo.ImageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        m_CreateInfo.MipMapCount = u32(std::log2(std::max(m_CreateInfo.Extent.width, m_CreateInfo.Extent.height))) + 1;    
    }
}

Image Image::Create(const Builder::CreateInfo& createInfo)
{
    Image image = {};
    
    switch (createInfo.SourceInfo)
    {
    case CreateInfo::SourceInfo::None:
    {
        image = AllocateImage(createInfo);
        if (createInfo.CreateView)
            image.m_ImageData.View = vkUtils::createImageView(Driver::DeviceHandle(), image.m_ImageData.Image, createInfo.Format, createInfo.ImageAspect, 1);
        break;
    }
    case CreateInfo::SourceInfo::ImageData:
    {
        image.m_ImageData = createInfo.ImageData;
        break;
    }
    case CreateInfo::SourceInfo::Asset:
    {
        image = CreateImageFromAsset(createInfo);
        break;
    }
    default:
        ASSERT(false, "Unrecognized image source info")
        std::unreachable();
    }

    image.m_ImageData.Aspect = createInfo.ImageAspect;
    
    return image;
}

void Image::Destroy(const Image& image)
{
    if (image.m_Allocation != VK_NULL_HANDLE)
    {
        vkDestroyImageView(Driver::DeviceHandle(), image.m_ImageData.View, nullptr);
        vmaDestroyImage(Driver::Allocator(), image.m_ImageData.Image, image.m_Allocation);
    }
}

ImageDescriptorInfo Image::CreateDescriptorInfo(VkFilter samplerFilter) const
{
    return CreateDescriptorInfo(samplerFilter, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

ImageDescriptorInfo Image::CreateDescriptorInfo(VkFilter samplerFilter, VkImageLayout imageLayout) const
{
    ImageDescriptorInfo descriptorTextureInfo = {};
    descriptorTextureInfo.Sampler = Texture::CreateSampler(samplerFilter, (f32)m_ImageData.MipMapCount); // todo: find a better place for it
    descriptorTextureInfo.Layout = imageLayout;
    descriptorTextureInfo.View = m_ImageData.View;

    return descriptorTextureInfo;
}

ImageDescriptorInfo Image::CreateDescriptorInfo() const
{
    ImageDescriptorInfo descriptorTextureInfo = {};
    descriptorTextureInfo.Sampler = VK_NULL_HANDLE;
    descriptorTextureInfo.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorTextureInfo.View = m_ImageData.View;

    return descriptorTextureInfo;
}

ImageSubresource Image::CreateSubresource() const
{
    ImageSubresource imageSubresource = {
        .Image = this,
        .Aspect = m_ImageData.Aspect};
    
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
        .Aspect = m_ImageData.Aspect,
        .MipMapBase = mipBase,
        .MipMapCount = mipCount,
        .LayerBase = layerBase,
        .LayerCount = layerCount};

    return imageSubresource;
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
        image = AllocateImage(createInfo);
        if (createInfo.CreateView)
        {
            ImageSubresource imageSubresource = image.CreateSubresource(0, 1, 0, 1);
            PrepareForTransfer(imageSubresource);
            CopyBufferToImage(createInfo.AssetInfo.Buffer, image, createInfo.ImageAspect);
            imageSubresource.MipMapCount = createInfo.MipMapCount;
            CreateMipMaps(image, createInfo);
            PrepareForShaderRead(imageSubresource);
            image.m_ImageData.View = vkUtils::createImageView(Driver::DeviceHandle(), image.m_ImageData.Image, createInfo.Format, createInfo.ImageAspect, createInfo.MipMapCount);
        }

        AssetManager::AddImage(createInfo.AssetInfo.AssetPath, image);
    }

    return image;
}

Image Image::AllocateImage(const CreateInfo& createInfo)
{
    Image image = {};

    image.m_ImageData.Width = createInfo.Extent.width;
    image.m_ImageData.Height = createInfo.Extent.height;
    image.m_ImageData.MipMapCount = createInfo.MipMapCount;
    image.m_ImageData.Aspect = createInfo.ImageAspect;
    
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.format = createInfo.Format;
    imageCreateInfo.usage = createInfo.ImageUsage;
    imageCreateInfo.extent = {.width = createInfo.Extent.width, .height = createInfo.Extent.height, .depth = 1};
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.mipLevels = createInfo.MipMapCount;
    imageCreateInfo.arrayLayers = 1;

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VulkanCheck(vmaCreateImage(Driver::Allocator(), &imageCreateInfo, &allocationInfo, &image.m_ImageData.Image, &image.m_Allocation, nullptr),
        "Failed to create image");

    return image;
}

void Image::PrepareForTransfer(const ImageSubresource& imageSubresource)
{
    PrepareImageGeneral(imageSubresource,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

void Image::PrepareForMipmap(const ImageSubresource& imageSubresource)
{
    PrepareImageGeneral(imageSubresource,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

void Image::PrepareForShaderRead(const ImageSubresource& imageSubresource)
{
    VkImageLayout current = imageSubresource.MipMapCount > 1 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    PrepareImageGeneral(imageSubresource,
       current, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
       VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void Image::PrepareImageGeneral(const ImageSubresource& imageSubresource,
        VkImageLayout current, VkImageLayout target,
        VkAccessFlags srcAccess, VkAccessFlags dstAccess,
        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
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

void Image::CopyBufferToImage(const Buffer& buffer, const Image& image, VkImageAspectFlags imageAspect)
{
    Driver::ImmediateUpload([&buffer, &image](const CommandBuffer& cmd)
    {
        RenderCommand::CopyBufferToImage(cmd, buffer, image);
    });
    
    Buffer::Destroy(buffer);
}

void Image::CreateMipMaps(const Image& image, const CreateInfo& createInfo)
{
    if (createInfo.MipMapCount == 1)
        return;

    ImageSubresource imageSubresource = image.CreateSubresource(0, 1, 0, 1);
    imageSubresource.Aspect = createInfo.ImageAspect;
    imageSubresource.LayerCount = 1;
    imageSubresource.MipMapCount = 1;
    
    PrepareForMipmap(imageSubresource);

    for (u32 i = 1; i < createInfo.MipMapCount; i++)
    {
        VkImageBlit2 imageBlit = {};
        imageBlit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        imageBlit.srcSubresource.aspectMask = createInfo.ImageAspect;
        imageBlit.srcSubresource.layerCount = 1;
        imageBlit.srcSubresource.mipLevel = i - 1;
        imageBlit.srcOffsets[1] = VkOffset3D{
            (i32)createInfo.Extent.width >> (i - 1),
            (i32)createInfo.Extent.height >> (i - 1),
            1};

        imageBlit.dstSubresource.aspectMask = createInfo.ImageAspect;
        imageBlit.dstSubresource.layerCount = 1;
        imageBlit.dstSubresource.mipLevel = i;
        imageBlit.dstOffsets[1] = VkOffset3D{
            (i32)createInfo.Extent.width  >> i,
            (i32)createInfo.Extent.height >> i,
            1};

        ImageSubresource mipmapSubresource = image.CreateSubresource(i, 1, 0, 1);

        PrepareForTransfer(mipmapSubresource);
    
        Driver::ImmediateUpload([&image, &imageBlit](const CommandBuffer& cmd)
        {
                ImageBlitInfo blitInfo = {};
                blitInfo.SourceImage = &image;
                blitInfo.DestinationImage = &image;
                blitInfo.ImageBlit = &imageBlit;
                blitInfo.Filter = VK_FILTER_LINEAR;

                RenderCommand::BlitImage(cmd, blitInfo);
        });
        PrepareForMipmap(mipmapSubresource);
    }
}

VkSampler Image::CreateSampler(VkFilter scaleFilter, f32 maxLod)
{
    VkSampler sampler;
    
    VkSamplerCreateInfo samplerCreateInfo = {};
    
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = scaleFilter;
    samplerCreateInfo.minFilter = scaleFilter;
    samplerCreateInfo.mipmapMode = vkUtils::mipmapModeFromSamplerFilter(scaleFilter);
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.maxAnisotropy = Driver::GetAnisotropyLevel();
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = maxLod;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    vkCreateSampler(Driver::DeviceHandle(), &samplerCreateInfo, nullptr, &sampler);
    Driver::DeletionQueue().AddDeleter([sampler](){ vkDestroySampler(Driver::DeviceHandle(), sampler, nullptr); });

    return sampler;
}
