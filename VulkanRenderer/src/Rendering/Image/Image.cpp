#include "rendererpch.h"

#include "Image.h"

#include "Vulkan/Device.h"

Span<const ImageSubresourceDescription> Image::GetAdditionalViews() const
{
    return Device::GetAdditionalImageViews(*this);
}

ImageViewHandle Image::GetViewHandle(ImageSubresourceDescription subresourceDescription) const
{
    return Device::GetImageViewHandle(*this, subresourceDescription);
}

const ImageDescription& Image::GetDescription() const
{
    return Device::GetImageDescription(*this);
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
