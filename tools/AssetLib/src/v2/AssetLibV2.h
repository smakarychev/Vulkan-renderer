#pragma once

#include "types.h"
#include "AssetId.h"
#include "Containers/Result.h"
#include "Containers/Span.h"

#include <string>
#include <vector>
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

namespace std
{
template <>
struct formatter<assetlib::io::IoError::ErrorCode> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(assetlib::io::IoError::ErrorCode code, format_context& ctx) const
    {
        switch (code)
        {
        case assetlib::io::IoError::ErrorCode::GeneralError:
            return format_to(ctx.out(), "GeneralError");
        case assetlib::io::IoError::ErrorCode::FailedToCreate:
            return format_to(ctx.out(), "FailedToCreate");
        case assetlib::io::IoError::ErrorCode::FailedToOpen:
            return format_to(ctx.out(), "FailedToOpen");
        case assetlib::io::IoError::ErrorCode::FailedToLoad:
            return format_to(ctx.out(), "FailedToLoad");
        case assetlib::io::IoError::ErrorCode::WrongFormat:
            return format_to(ctx.out(), "WrongFormat");
        default:
            return format_to(ctx.out(), "Unknown error");
        }
    }
};
template <>
struct formatter<assetlib::io::IoError> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const assetlib::io::IoError& error, format_context& ctx) const
    {
        return format_to(ctx.out(), "{} ({})", error.Message, error.Code);
    }
};
}

#define ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, ...) \
    if (!(x)) { return std::unexpected(::assetlib::io::IoError{.Code = error, .Message = std::format(__VA_ARGS__)}); }
