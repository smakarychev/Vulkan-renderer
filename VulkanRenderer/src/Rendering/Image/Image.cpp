#include "rendererpch.h"

#include "Image.h"

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
