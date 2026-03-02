#include "SeparateAssetIoInterface.h"

#include <fstream>

namespace fs = std::filesystem;

namespace lux::assetlib::io
{
IoResult<void> SeparateAssetIoInterface::WriteHeader(const AssetFile& file)
{
    const fs::path& headerPath = file.IoInfo.HeaderFile;
    const fs::path& binaryPath = file.IoInfo.BinaryFile;

    ASSETLIB_CHECK_RETURN_IO_ERROR(!headerPath.empty(), IoError::ErrorCode::FailedToCreate,
        "Assetlib: File paths are not set: header: {}", headerPath.string())

    if (fs::exists(binaryPath))
        fs::remove(binaryPath);

    bool success = fs::exists(fs::path(headerPath).parent_path()) ||
        fs::create_directories(fs::path(headerPath).parent_path());
    ASSETLIB_CHECK_RETURN_IO_ERROR(success, IoError::ErrorCode::FailedToCreate,
        "Assetlib: Failed to create header directory: {}", headerPath.string())

    std::ofstream headerOut(headerPath, std::ios::out);
    ASSETLIB_CHECK_RETURN_IO_ERROR(headerOut.good(), IoError::ErrorCode::FailedToCreate,
        "Assetlib: Failed to create header file: {}", headerPath.string())

    const auto assetHeaderString = getAssetFullHeaderFormattedString(file);
    ASSETLIB_CHECK_RETURN_IO_ERROR(assetHeaderString.has_value(), IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to create header string: {} ({})", assetHeaderString.error().Message, headerPath.string())
    headerOut.write(assetHeaderString->c_str(), (isize)assetHeaderString->size());

    return {};
}

IoResult<u64> SeparateAssetIoInterface::WriteBinaryChunk(const AssetFile& file, Span<const std::byte> binaryDataChunk)
{
    const fs::path& headerPath = file.IoInfo.HeaderFile;
    const fs::path& binaryPath = file.IoInfo.BinaryFile;

    ASSETLIB_CHECK_RETURN_IO_ERROR(!binaryPath.empty(), IoError::ErrorCode::FailedToCreate,
        "Assetlib: File paths are not set: binary: {}", binaryPath.string())

    if (binaryPath != headerPath)
    {
        const bool success = fs::exists(fs::path(binaryPath).parent_path()) ||
            fs::create_directories(fs::path(binaryPath).parent_path());
        ASSETLIB_CHECK_RETURN_IO_ERROR(success, IoError::ErrorCode::FailedToCreate,
            "Assetlib: Failed to create binary directory: {}", binaryPath.string())
    }

    std::ofstream binaryOut(binaryPath, std::ios::binary | std::ios::ate);
    ASSETLIB_CHECK_RETURN_IO_ERROR(binaryOut.good(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open binary file: {}", binaryPath.string())

    binaryOut.write((const char*)binaryDataChunk.data(), (isize)binaryDataChunk.size());

    return binaryDataChunk.size();
}

IoResult<AssetFile> SeparateAssetIoInterface::ReadHeader(const std::filesystem::path& headerPath)
{
    auto assetHeader = unpackBaseAssetHeader(headerPath);
    ASSETLIB_CHECK_RETURN_IO_ERROR(assetHeader.has_value(), IoError::ErrorCode::WrongFormat,
        "Assetlib: Failed to parse header file: {} ({})", assetHeader.error().Message, headerPath.string())

    return assetHeader;
}

IoResult<void> SeparateAssetIoInterface::ReadBinaryChunk(const AssetFile& file, std::byte* destination, u64 offsetBytes,
    u64 sizeBytes)
{
    const fs::path& binaryPath = file.IoInfo.BinaryFile;

    std::ifstream binaryIn(binaryPath, std::ios::binary | std::ios::ate);
    ASSETLIB_CHECK_RETURN_IO_ERROR(binaryIn.good(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open binary file: {}", binaryPath.string())

    const isize totalFileSize = binaryIn.tellg();
    ASSETLIB_CHECK_RETURN_IO_ERROR(totalFileSize - offsetBytes >= sizeBytes,
        IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed to read binary chunk: total file size is less then offset + size: {}", binaryPath.string())

    binaryIn.seekg((isize)offsetBytes, std::ios::beg);
    binaryIn.read((char*)destination, (isize)sizeBytes);

    return {};
}

std::string SeparateAssetIoInterface::GetHeaderExtension(std::string_view preferred) const
{
    return std::string{preferred};
}

std::string SeparateAssetIoInterface::GetBinariesExtension() const
{
    return "bin";
}
}
