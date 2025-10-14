#pragma once
#include <string>
#include <vector>

#include "types.h"

namespace assetLib
{
    static constexpr u32 JSON_INDENT = 4;
    enum class AssetType : u32
    {
        Texture, Model, Shader
    };
    
    struct File
    {
        std::string JSON;
        std::vector<u8> Blob;
    };

    using AssetBlob = std::vector<std::byte>;

    enum class CompressionMode : u32
    {
        None,
        LZ4,
    };
    
    struct AssetInfoBase
    {
        std::string OriginalFile;
        std::string BlobFile;
        AssetType Type;
        u32 Version;
        CompressionMode CompressionMode;
        u64 BlobSizeBytes;
        AssetBlob Blob;
    };
    
    bool saveAssetFile(std::string_view assetPath, std::string_view blobPath, const File& file);
    bool loadAssetFile(std::string_view assetPath, File& file);

    void packAssetInfo(const AssetInfoBase& assetInfo, void* metadata);
    void unpackAssetInfo(AssetInfoBase& assetInfo, const void* metadata);
    
    CompressionMode parseCompressionModeString(std::string_view modeString);
    AssetType parseAssetTypeString(std::string_view assetType);
    std::string_view assetTypeToString(AssetType assetType);
}

