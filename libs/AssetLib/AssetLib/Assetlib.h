#pragma once

#include "AssetId.h"
#include <CoreLib/types.h>
#include <CoreLib/Containers/Guid.h>

#include <string>
#include <filesystem>

namespace lux::assetlib
{
using AssetCustomHeaderType = std::string;

using AssetType = Guid;

static constexpr std::string_view ASSETLIB_METADATA_EXTENSION = ".meta";
struct AssetTypeMetadata
{
    AssetType Type{};
    std::string Name{};
    u32 Version{1};
};

struct AssetIoMetadata
{
    std::string OriginalFile{};
    std::filesystem::path HeaderFile{};
    std::filesystem::path BinaryFile{};
    u64 HeaderSizeBytes{};
    u64 BinarySizeBytes{};
    u64 BinarySizeBytesCompressed{};
    std::vector<u64> BinarySizeBytesChunksCompressed{};
    std::string IoMode{};
    std::string CompressionMode{};
    Guid IoGuid{};
    Guid CompressionGuid{};
};

struct AssetMetadata
{
    AssetId AssetId{};
    AssetTypeMetadata Type{};
    AssetIoMetadata Io{};
};

struct AssetPacked
{
    AssetCustomHeaderType Header{};
    std::vector<std::byte> PackedBinaries{};
    std::vector<u64> PackedBinarySizeBytesChunks{};
};

bool isMetadataPath(const std::filesystem::path& path);
std::filesystem::path getMetadataPath(const std::filesystem::path& path);
std::string getMetadataRawExtension(const std::filesystem::path& path);
}

