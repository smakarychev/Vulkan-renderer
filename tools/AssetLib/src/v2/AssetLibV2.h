#pragma once

#include "types.h"
#include "AssetId.h"
#include "Containers/Guid.h"

#include <string>
#include <filesystem>

namespace lux::assetlib
{
constexpr u32 JSON_INDENT = 2;

using AssetCustomHeaderType = std::string;

using AssetType = Guid;

struct AssetMetadata
{
    AssetId AssetId{};
    AssetType Type{};
    std::string TypeName{};
    u32 Version{1};
};

struct AssetFileIoInfo
{
    std::string OriginalFile{};
    std::filesystem::path HeaderFile{};
    std::filesystem::path BinaryFile{};
    u64 HeaderSizeBytes{};
    u64 BinarySizeBytes{};
    u64 BinarySizeBytesCompressed{};
    std::vector<u64> BinarySizeBytesChunksCompressed{};
    std::string CompressionMode{};
    Guid CompressionGuid{};
};

struct AssetPacked
{
    AssetMetadata Metadata{};
    AssetCustomHeaderType AssetSpecificInfo{};
    std::vector<std::byte> PackedBinaries{};
    std::vector<u64> PackedBinarySizeBytesChunks{};
};

struct AssetFile
{
    AssetFileIoInfo IoInfo{};
    AssetMetadata Metadata{};
    AssetCustomHeaderType AssetSpecificInfo{};
};
}

