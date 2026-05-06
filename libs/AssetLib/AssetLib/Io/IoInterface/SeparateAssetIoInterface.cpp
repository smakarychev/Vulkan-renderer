#include "SeparateAssetIoInterface.h"

#include <CoreLib/Utils/FileUtils.h>

#include <fstream>

namespace fs = std::filesystem;

namespace lux::assetlib::io
{
IoResult<u64> SeparateAssetIoInterface::WriteHeader(const AssetMetadata& metadata, const AssetCustomHeaderType& header)
{
    const fs::path& headerPath = metadata.Io.HeaderFile;
    const fs::path& binaryPath = metadata.Io.BinaryFile;

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

    const auto assetHeaderString = getAssetHeaderFormatted(header);
    headerOut.write(assetHeaderString.c_str(), (isize)assetHeaderString.size());

    return assetHeaderString.size();
}

IoResult<u64> SeparateAssetIoInterface::WriteBinaryChunk(const AssetMetadata& metadata, 
    Span<const std::byte> binaryDataChunk)
{
    const fs::path& headerPath = metadata.Io.HeaderFile;
    const fs::path& binaryPath = metadata.Io.BinaryFile;

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

IoResult<AssetCustomHeaderType> SeparateAssetIoInterface::ReadHeader(const AssetMetadata& metadata)
{
    auto headerRead = readFileToString(metadata.Io.HeaderFile);
    ASSETLIB_CHECK_RETURN_IO_ERROR(headerRead.has_value(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open header file: {}", metadata.Io.HeaderFile.string())

    return *headerRead;
}

IoResult<void> SeparateAssetIoInterface::ReadBinaryChunk(const AssetMetadata& metadata, std::byte* destination, 
    u64 offsetBytes, u64 sizeBytes)
{
    const fs::path& binaryPath = metadata.Io.BinaryFile;

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
