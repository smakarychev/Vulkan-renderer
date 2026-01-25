#include "CombinedAssetIoInterface.h"

#include <fstream>

namespace fs = std::filesystem;

namespace lux::assetlib::io
{
namespace
{
constexpr std::string_view ASSET_COMBINED_FILE_MAGIC = "ASSETBFF";
constexpr std::string_view ASSET_COMBINED_FILE_EXTENSION = "gbin";
constexpr u32 ASSET_COMBINED_FILE_MAGIC_LENGTH = (u32)ASSET_COMBINED_FILE_MAGIC.length();
static_assert(ASSET_COMBINED_FILE_MAGIC_LENGTH == 8);
}

IoResult<void> CombinedAssetIoInterface::WriteHeader(const AssetFile& file)
{
    ASSETLIB_CHECK_RETURN_IO_ERROR(file.IoInfo.HeaderFile == file.IoInfo.BinaryFile, IoError::ErrorCode::FailedToCreate,
        "Assetlib: File paths for combined assets have to be equal")

    const fs::path& path = file.IoInfo.HeaderFile;
    ASSETLIB_CHECK_RETURN_IO_ERROR(!path.empty(), IoError::ErrorCode::FailedToCreate, "Assetlib: File path is not set")

    const bool success = fs::exists(fs::path(path).parent_path()) ||
        fs::create_directories(fs::path(path).parent_path());
    ASSETLIB_CHECK_RETURN_IO_ERROR(success, IoError::ErrorCode::FailedToCreate,
        "Assetlib: Failed to create path directory: {}", path.string())

    std::ofstream out(path, std::ios::out | std::ios::binary);
    ASSETLIB_CHECK_RETURN_IO_ERROR(out.good(), IoError::ErrorCode::FailedToCreate,
        "Assetlib: Failed to create file: {}", path.string())

    const auto assetHeaderString = getAssetFullHeaderString(file);
    ASSETLIB_CHECK_RETURN_IO_ERROR(assetHeaderString.has_value(), IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to create header string: {} ({})", assetHeaderString.error().Message, path.string())
    out.write(ASSET_COMBINED_FILE_MAGIC.data(), ASSET_COMBINED_FILE_MAGIC.length());
    out.write((const char*)&file.Metadata.Version, sizeof file.Metadata.Version);

    const isize headerSizeBytes = (isize)assetHeaderString->size();
    out.write((const char*)&headerSizeBytes, sizeof headerSizeBytes);

    const isize binarySizeBytes = (isize)file.IoInfo.BinarySizeBytesCompressed;
    out.write((const char*)&binarySizeBytes, sizeof binarySizeBytes);

    out.write(assetHeaderString->data(), headerSizeBytes);

    return {};
}

IoResult<u64> CombinedAssetIoInterface::WriteBinaryChunk(const AssetFile& file,
    Span<const std::byte> binaryDataChunk)
{
    ASSETLIB_CHECK_RETURN_IO_ERROR(file.IoInfo.HeaderFile == file.IoInfo.BinaryFile, IoError::ErrorCode::FailedToCreate,
        "Assetlib: File paths for combined assets have to be equal")

    const fs::path& path = file.IoInfo.HeaderFile;
    ASSETLIB_CHECK_RETURN_IO_ERROR(!path.empty(), IoError::ErrorCode::FailedToCreate, "Assetlib: File path is not set")

    std::ofstream out(path, std::ios::binary | std::ios::ate);
    ASSETLIB_CHECK_RETURN_IO_ERROR(out.good(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open file: {}", path.string())
    ASSETLIB_CHECK_RETURN_IO_ERROR(out.tellp() > 0, IoError::ErrorCode::GeneralError,
        "AssetLib: File header is not written")

    out.write((const char*)binaryDataChunk.data(), (isize)binaryDataChunk.size());

    return binaryDataChunk.size();
}

IoResult<AssetFile> CombinedAssetIoInterface::ReadHeader(const std::filesystem::path& headerPath)
{
    std::ifstream in(headerPath, std::ios::binary);
    ASSETLIB_CHECK_RETURN_IO_ERROR(in.good(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open combined asset file: {}", headerPath.string())

    std::string assetMagicString = std::string(ASSET_COMBINED_FILE_MAGIC);
    in.read(assetMagicString.data(), (isize)assetMagicString.size());
    ASSETLIB_CHECK_RETURN_IO_ERROR(assetMagicString == ASSET_COMBINED_FILE_MAGIC, IoError::ErrorCode::WrongFormat,
        "Assetlib: Combined asset file does not include expected magic. Expected {}, got {}",
        ASSET_COMBINED_FILE_MAGIC, assetMagicString)

    u32 version = 0;
    in.read((char*)&version, sizeof version);

    ASSETLIB_CHECK_RETURN_IO_ERROR(version == ASSET_CURRENT_VERSION, IoError::ErrorCode::WrongFormat,
        "Assetlib: Combined asset file has unexpected version. Expected {}, got {}",
        ASSET_CURRENT_VERSION, version)

    isize headerSizeBytes = 0;
    in.read((char*)&headerSizeBytes, sizeof headerSizeBytes);

    isize binarySizeBytes = 0;
    in.read((char*)&binarySizeBytes, sizeof binarySizeBytes);

    std::string headerString(headerSizeBytes, 0);
    in.read(headerString.data(), headerSizeBytes);

    auto assetFile = unpackBaseAssetHeaderFromBuffer(headerString);
    ASSETLIB_CHECK_RETURN_IO_ERROR(assetFile.has_value(), IoError::ErrorCode::WrongFormat,
        "Assetlib: Failed to parse header file: {} ({})", assetFile.error().Message, headerPath.string())

    assetFile->IoInfo.HeaderSizeBytes = headerSizeBytes;
    assetFile->IoInfo.BinarySizeBytesCompressed = binarySizeBytes;

    return assetFile;
}

IoResult<void> CombinedAssetIoInterface::ReadBinaryChunk(const AssetFile& file, std::byte* destination, u64 offsetBytes,
    u64 sizeBytes)
{
    const isize headerSizeBytes = (isize)file.IoInfo.HeaderSizeBytes;
    const fs::path& binaryPath = file.IoInfo.BinaryFile;

    std::ifstream binaryIn(binaryPath, std::ios::binary);
    ASSETLIB_CHECK_RETURN_IO_ERROR(binaryIn.good(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open binary file: {}", binaryPath.string())
    const isize totalFileSize = binaryIn.tellg();
    ASSETLIB_CHECK_RETURN_IO_ERROR(totalFileSize - offsetBytes - headerSizeBytes >= sizeBytes,
        IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed to read binary chunk: total file size is less then offset + size: {}", binaryPath.string())

    binaryIn.seekg((isize)(
        ASSET_COMBINED_FILE_MAGIC_LENGTH +
        sizeof file.Metadata.Version +
        sizeof file.IoInfo.HeaderSizeBytes +
        file.IoInfo.HeaderSizeBytes +
        offsetBytes), std::ios::beg);
    binaryIn.read((char*)destination, (isize)sizeBytes);

    return {};
}

std::string CombinedAssetIoInterface::GetHeaderExtension(std::string_view preferred) const
{
    return std::string{ASSET_COMBINED_FILE_EXTENSION};
}

std::string CombinedAssetIoInterface::GetBinariesExtension() const
{
    return std::string{ASSET_COMBINED_FILE_EXTENSION};
}
}
