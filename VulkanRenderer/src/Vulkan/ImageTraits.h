#pragma once

#include "types.h"
#include "Core/core.h"

enum class ImageFormat
{
    Undefined = 0,

    R8_UNORM, 
    R8_SNORM, 
    R8_UINT, 
    R8_SINT, 
    R8_SRGB,

    RG8_UNORM, 
    RG8_SNORM, 
    RG8_UINT, 
    RG8_SINT, 
    RG8_SRGB,

    RGBA8_UNORM, 
    RGBA8_SNORM, 
    RGBA8_UINT, 
    RGBA8_SINT, 
    RGBA8_SRGB,
    
    R16_UNORM, 
    R16_SNORM, 
    R16_UINT, 
    R16_SINT, 
    R16_FLOAT,

    RG16_UNORM, 
    RG16_SNORM, 
    RG16_UINT, 
    RG16_SINT, 
    RG16_FLOAT,
    
    RGBA16_UNORM, 
    RGBA16_SNORM, 
    RGBA16_UINT, 
    RGBA16_SINT, 
    RGBA16_FLOAT,
    
    R32_UINT, 
    R32_SINT, 
    R32_FLOAT,

    RG32_UINT, 
    RG32_SINT, 
    RG32_FLOAT,

    RGBA32_UINT, 
    RGBA32_SINT, 
    RGBA32_FLOAT,
   
    RGB10A2,
    R11G11B10,

    D32_FLOAT,
    D24_UNORM_S8_UINT,
    D32_FLOAT_S8_UINT,
  
    MaxVal
};

enum class ImageLayout
{
    Undefined = 0,
    
    General,
    
    Attachment,
    ReadOnly,
    
    ColorAttachment,
    Present,
    
    DepthStencilAttachment,
    DepthStencilReadonly,
    DepthAttachment,
    DepthReadonly,
    
    Source,
    Destination
};

enum class ImageKind
{
    Image2d, Image3d //, Cubemap
};

enum class ImageUsage
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
};

CREATE_ENUM_FLAGS_OPERATORS(ImageUsage)

enum class ImageFilter
{
    Linear, Nearest
};

enum class SamplerWrapMode
{
    ClampEdge, ClampBorder, Repeat
};

enum class SamplerReductionMode
{
    Average, Min, Max
};

