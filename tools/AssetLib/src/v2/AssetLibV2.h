#pragma once

#include <string>
#include <vector>

#include "types.h"
#include "AssetId.h"
#include "Containers/Result.h"
#include "Containers/Span.h"

#include <filesystem>

namespace assetlib
{
constexpr u32 JSON_INDENT = 2;

using AssetCustomHeaderType = std::string;

using AssetType = u32;

enum class CompressionMode : u32
{
    Raw,
    LZ4,
};

struct AssetMetadata
{
    AssetId AssetId{};
    AssetType Type{};
    std::string TypeName{};
    u32 Version{1};
    std::string OriginalFile{};
};

enum class AssetFileIoType : u8
{
    Separate, Combined
};

struct AssetFileIoInfo
{
    std::filesystem::path HeaderFile{};
    std::filesystem::path BinaryFile{};
    u64 HeaderSizeBytes{};
    u64 BinarySizeBytes{};
    u64 BinarySizeBytesCompressed{};
    CompressionMode CompressionMode{CompressionMode::Raw};
};

struct AssetFile
{
    AssetFileIoInfo IoInfo{};
    AssetMetadata Metadata;
    AssetCustomHeaderType AssetSpecificInfo;
};

using AssetBinary = std::vector<std::byte>;

struct AssetFileAndBinary
{
    AssetFile File{};
    AssetBinary Binary{};
};

namespace io
{
struct IoError
{
    enum class ErrorCode : u8
    {
        GeneralError,
        FailedToCreate,
        FailedToOpen,
        FailedToLoad,
        WrongFormat,
    };
    ErrorCode Code{ErrorCode::FailedToCreate};
    std::string Message;
};

template <typename T>
using IoResult = Result<T, IoError>;

IoResult<void> saveAssetFile(const AssetFile& file, Span<const std::byte> binaryData);
IoResult<AssetFileAndBinary> loadAssetFile(const std::filesystem::path& headerPath);
IoResult<AssetFile> loadAssetFileHeader(const std::filesystem::path& headerPath);
IoResult<AssetBinary> loadAssetFileBinaries(const AssetFile& file);
IoResult<AssetBinary> loadAssetFileBinaries(const AssetFile& file, u64 offsetBytes, u64 sizeBytes);

IoResult<void> saveAssetFileCombined(const AssetFile& file, Span<const std::byte> binaryData);
IoResult<AssetFileAndBinary> loadAssetFileCombined(const std::filesystem::path& path);
IoResult<AssetFile> loadAssetFileCombinedHeader(const std::filesystem::path& path);
IoResult<AssetBinary> loadAssetFileCombinedBinaries(const AssetFile& file);
IoResult<AssetBinary> loadAssetFileCombinedBinaries(const AssetFile& file, u64 offsetBytes, u64 sizeBytes);
}

}

#define ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, ...) \
    if (!(x)) { return std::unexpected(::assetlib::io::IoError{.Code = error, .Message = std::format(__VA_ARGS__)}); }
