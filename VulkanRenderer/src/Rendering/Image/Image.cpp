#include "rendererpch.h"

#include "Image.h"

#include "Vulkan/Device.h"

ImageSubresourceDescription Image::GetAdditionalView(u32 viewIndex) const
{
    return Device::GetAdditionalImageView(*this, viewIndex);
}

ImageViewHandle Image::GetViewHandle(ImageSubresourceDescription subresourceDescription) const
{
    return Device::GetImageViewHandle(*this, subresourceDescription);
}

ImageDescription Image::GetDescriptionCopy() const
{
    return Device::GetImageDescription(*this);
}

i8 Image::GetMipmaps() const
{
    return Device::GetImageMipmaps(*this);
}

glm::uvec2 Image::GetSize2d() const
{
    return Device::GetImageSize2d(*this);
}

glm::uvec3 Image::GetSize3d() const
{
    return Device::GetImageSize3d(*this);
}

ImageUsage Image::GetUsage() const
{
    return Device::GetImageUsage(*this);
}

u32 ImageDescription::GetDepth() const
{
    const bool is3dImage = Kind == ImageKind::Image3d;
    return is3dImage ? LayersDepth : 1;
}

u32 ImageDescription::GetDepth(i8 mip) const
{
    const bool is3dImage = Kind == ImageKind::Image3d;
    return is3dImage ? LayersDepth >> mip : 1;
}

i8 ImageDescription::GetLayers() const
{
    const bool is3dImage = Kind == ImageKind::Image3d;
    return is3dImage ? (i8)1 : (i8)LayersDepth;
}
