#pragma once

#include "Core/core.h"

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
    
    NoDeallocation = BIT(9),
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

enum class AttachmentLoad
{
    Unspecified = 0,
    
    Load,
    Clear
};

enum class AttachmentStore
{
    Unspecified = 0,
    
    Store
};