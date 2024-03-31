#pragma once

#include "Core/core.h"
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
    Image2d, Image3d, Cubemap
};

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

enum class SamplerReductionMode : u8
{
    Average, Min, Max
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