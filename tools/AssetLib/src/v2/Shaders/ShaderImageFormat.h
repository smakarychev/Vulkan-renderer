#pragma once
#include "types.h"

namespace assetlib
{
enum class ShaderImageFormat : u8
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

    RGB32_UINT, 
    RGB32_SINT, 
    RGB32_FLOAT,
    
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
}
