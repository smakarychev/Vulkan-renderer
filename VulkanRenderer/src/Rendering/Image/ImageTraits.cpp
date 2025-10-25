#include "rendererpch.h"

#include "ImageTraits.h"

namespace ImageTraits
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
}