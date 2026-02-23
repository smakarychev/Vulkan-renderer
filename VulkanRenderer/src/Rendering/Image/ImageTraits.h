#pragma once

#include "core.h"
#include "types.h"

enum class ImageLayout : u8
{
    Undefined = 0,
    
    General,
    
    Attachment,
    Readonly,
    
    ColorAttachment,
    Present,
    
    DepthStencilAttachment,
    DepthStencilReadonly,
    DepthAttachment,
    DepthReadonly,
    
    Source,
    Destination
};

enum class ImageKind : u8
{
    Image2d,
    Image3d,
    ImageCubemap,
    Image2dArray,
};

enum class ImageViewKind : u8
{
    Image2d,
    Image3d,
    Cubemap,
    Image2dArray,
    /* inherit view kind from image kind */
    Inherit,
};
static_assert((u8)ImageViewKind::Image2d == (u8)ImageKind::Image2d,
    "Enum values of image kind and image view kind have to match");
static_assert((u8)ImageViewKind::Image3d == (u8)ImageKind::Image3d,
    "Enum values of image kind and image view kind have to match");
static_assert((u8)ImageViewKind::Cubemap == (u8)ImageKind::ImageCubemap,
    "Enum values of image kind and image view kind have to match");
static_assert((u8)ImageViewKind::Image2dArray == (u8)ImageKind::Image2dArray,
    "Enum values of image kind and image view kind have to match");

enum class ImageUsage : u16
{
    None = 0,
    Sampled = BIT(1),
    Color = BIT(2),
    Depth = BIT(3),
    Stencil = BIT(4),
    Storage = BIT(5),
    Readback = BIT(6),
    Source = BIT(7),
    Destination = BIT(8),
    
    NoDeallocation = BIT(9),
};

CREATE_ENUM_FLAGS_OPERATORS(ImageUsage)

enum class ImageFilter : u8
{
    Linear, Nearest
};

enum class SamplerWrapMode : u8
{
    ClampEdge, ClampBorder, Repeat
};

enum class SamplerBorderColor : u8
{
    White, Black
};

enum class SamplerReductionMode : u8
{
    Average, Min, Max
};

enum class SamplerDepthCompareMode : u8
{
    None, Greater, Less
};

enum class AttachmentLoad : u8
{
    Unspecified = 0,
    
    Load,
    Clear
};

enum class AttachmentStore : u8
{ 
    Unspecified = 0,
    
    Store
};

namespace ImageTraits
{
    std::string imageKindToString(ImageKind kind);
    std::string imageViewKindToString(ImageViewKind kind);
    std::string imageUsageToString(ImageUsage usage);
    std::string imageFilterToString(ImageFilter filter);
    std::string imageLayoutToString(ImageLayout layout);
}