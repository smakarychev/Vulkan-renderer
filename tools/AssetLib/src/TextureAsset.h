﻿#pragma once
#include "AssetLib.h"
#include "types.h"

namespace assetLib
{
    enum class TextureFormat : u32
    {
        Unknown, SRGBA8
    };
    
    struct TextureDimensions
    {
        u32 Width;
        u32 Height;
        u32 Depth;
    };
    
    struct TextureInfo : AssetInfoBase
    {
        TextureFormat Format;
        u64 SizeBytes;
        TextureDimensions Dimensions;
    };

    TextureInfo readTextureInfo(const assetLib::File& file);

    assetLib::File packTexture(const TextureInfo& info, void* pixels);
    void unpackTexture(TextureInfo& info, const u8* source, u64 sourceSizeBytes, u8* destination);
}