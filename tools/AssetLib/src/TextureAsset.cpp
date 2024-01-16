#include "TextureAsset.h"

#include <iostream>
#include <nlm_json.hpp>
#include <lz4.h>

#include "utils.h"

namespace
{
    assetLib::TextureFormat parseFormatString(std::string_view format)
    {
        if (format == "SRGBA8")
            return assetLib::TextureFormat::SRGBA8;
        if (format == "RGBA8")
            return assetLib::TextureFormat::RGBA8;
        if (format == "unknown")
            return assetLib::TextureFormat::Unknown;
        std::cout << "Unrecognized texture format string\n";
        std::unreachable();
    }

    std::string textureFormatToString(assetLib::TextureFormat format)
    {
        switch (format)
        {
        case assetLib::TextureFormat::SRGBA8:
            return "SRGBA8";
        case assetLib::TextureFormat::RGBA8:
            return "RGBA8";
        case assetLib::TextureFormat::Unknown:
            return "unknown";
        default:
            std::cout << "Unsupported texture format\n";
            break;
        }
        std::unreachable();
    }
}

namespace assetLib
{
    TextureInfo readTextureInfo(const assetLib::File& file)
    {
        TextureInfo info = {};
        
        nlohmann::json metadata = nlohmann::json::parse(file.JSON);

        std::string formatString = metadata["format"];
        info.Format = parseFormatString(formatString);

        info.Dimensions.Width = metadata["width"];
        info.Dimensions.Height = metadata["height"];
        info.Dimensions.Depth = metadata["depth"];
        info.SizeBytes = metadata["buffer_size"];

        unpackAssetInfo(info, &metadata);
        
        return info;
    }

    assetLib::File packTexture(const TextureInfo& info, const void* pixels)
    {
        nlohmann::json metadata;
        
        metadata["format"] =  textureFormatToString(info.Format);
        metadata["width"] = info.Dimensions.Width;
        metadata["height"] = info.Dimensions.Height;
        metadata["depth"] = info.Dimensions.Depth;
        metadata["buffer_size"] = info.SizeBytes;

        packAssetInfo(info, &metadata);
    
        assetLib::File assetFile = {};

        u64 blobSizeBytes = utils::compressToBlob(assetFile.Blob, pixels, info.SizeBytes);
        metadata["asset"]["blob_size_bytes"] = blobSizeBytes;
        metadata["asset"]["type"] = assetTypeToString(AssetType::Texture);

        assetFile.JSON = metadata.dump(JSON_INDENT);
        
        return assetFile;
    }

    void unpackTexture(TextureInfo& info, const u8* source, u64 sourceSizeBytes, u8* destination)
    {
        if (info.CompressionMode == CompressionMode::LZ4 && sourceSizeBytes != info.SizeBytes)
            LZ4_decompress_safe((const char*)source, (char*)destination, (i32)sourceSizeBytes, (i32)info.SizeBytes);
        else
            memcpy(destination, source, info.SizeBytes);
    }
}
