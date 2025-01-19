#include "AssetLib.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlm_json.hpp>

#include "core.h"

namespace assetLib
{
    bool saveAssetFile(std::string_view assetPath, std::string_view blobPath, const File& file)
    {
        std::filesystem::create_directories(std::filesystem::path(assetPath).parent_path());
        std::ofstream jsonOut(assetPath.data(), std::ios::out);
        std::ofstream blobOut(blobPath.data(), std::ios::binary | std::ios::out);

        jsonOut.write(file.JSON.data(), (isize)file.JSON.size());
        blobOut.write((const char*)file.Blob.data(), (isize)file.Blob.size());
        
        return true;
    }

    bool loadAssetFile(std::string_view assetPath, File& file)
    {
        std::ifstream jsonIn(assetPath.data(), std::ios::ate);
        if (!jsonIn.is_open())
        {
            LOG("AssetLib error: failed to open file {}", assetPath);
            return false;
        }

        isize jsonSizeBytes = jsonIn.tellg();
        jsonIn.seekg(0);
        file.JSON.resize(jsonSizeBytes);
        jsonIn.read(file.JSON.data(), jsonSizeBytes);
        
        nlohmann::json metadata = nlohmann::json::parse(file.JSON);

        isize blobSizeBytes = metadata["asset"]["blob_size_bytes"];
        std::string blobPath = metadata["asset"]["blob_file"];
        file.Blob.resize(blobSizeBytes);

        std::ifstream blobIn(blobPath, std::ios::binary);
        blobIn.read((char *)file.Blob.data(), blobSizeBytes);

        return true;
    }

    void packAssetInfo(const AssetInfoBase& assetInfo, void* metadata)
    {
        nlohmann::json assetMetadata;
        assetMetadata["compression"] = "LZ4";
        assetMetadata["original_file"] = assetInfo.OriginalFile;
        assetMetadata["blob_file"] = assetInfo.BlobFile;
        assetMetadata["version"] = 1;
        (*(nlohmann::json*)metadata)["asset"] = assetMetadata; 
    }

    void unpackAssetInfo(AssetInfoBase& assetInfo, const void* metadata)
    {
        const nlohmann::json& assetMetadata = (*(const nlohmann::json*)metadata)["asset"];
        std::string compressionString = assetMetadata["compression"];
        assetInfo.CompressionMode = parseCompressionModeString(compressionString);
        assetInfo.OriginalFile = assetMetadata["original_file"];
        assetInfo.BlobFile = assetMetadata["blob_file"];
        assetInfo.BlobSizeBytes = assetMetadata["blob_size_bytes"];
        std::string assetTypeString = assetMetadata["type"];
        assetInfo.Type = parseAssetTypeString(assetTypeString);
        assetInfo.Version = assetMetadata["version"];
    }

    CompressionMode parseCompressionModeString(std::string_view modeString)
    {
        if (modeString == "LZ4")
            return CompressionMode::LZ4;
        if (modeString == "None")
            return CompressionMode::None;
        
        std::unreachable();
    }

    AssetType parseAssetTypeString(std::string_view assetType)
    {
        if (assetType == "Texture")
            return AssetType::Texture;
        if (assetType == "Model")
            return AssetType::Model;
        if (assetType == "Shader")
            return AssetType::Shader;
        
        std::cout << std::format("Unrecognized asset type: {}\n", assetType);
        std::unreachable();
    }

    std::string_view assetTypeToString(AssetType assetType)
    {
        switch (assetType)
        {
        case AssetType::Texture:    return "Texture";
        case AssetType::Model:      return "Model";
        case AssetType::Shader:     return "Shader";
        default:
            std::cout << std::format("Unsupported asset type\n");
            break;
        }
        
        std::unreachable();
    }
}
