#include "TextureAsset.h"

#include <nlm_json.hpp>
#include <lz4.h>

#include "utils.h"

namespace
{
    assetLib::TextureFormat parseFormatString(std::string_view format)
    {
        if (format == "SRGBA8")
            return assetLib::TextureFormat::SRGBA8;
        return assetLib::TextureFormat::Unknown;
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
        
        metadata["format"] = "SRGBA8";
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
