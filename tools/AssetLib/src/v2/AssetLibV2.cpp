#include "AssetLibV2.h"

#include "core.h"
#include "Reflection/AssetLibReflectionUtility.inl"

#include <fstream>

template <>
struct glz::meta<assetlib::CompressionMode> : assetlib::reflection::CamelCase {
    using enum assetlib::CompressionMode;
    static constexpr auto value = glz::enumerate(Raw, LZ4);
};
template <>
struct glz::meta<assetlib::AssetFileIoType> : assetlib::reflection::CamelCase {
    using enum assetlib::AssetFileIoType;
    static constexpr auto value = glz::enumerate(Separate, Combined);
};
template <> struct ::glz::meta<assetlib::AssetFileIoInfo> : assetlib::reflection::CamelCase {}; 
template <> struct ::glz::meta<assetlib::AssetMetadata> : assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<assetlib::AssetFile> : assetlib::reflection::CamelCase {
    using T = assetlib::AssetFile;
    static constexpr auto READ_H = [](T& file, glz::raw_json&& input) { file.AssetSpecificInfo = std::move(input.str); };
    static constexpr auto WRITE_H = [](auto& file) -> glz::raw_json_view { return file.AssetSpecificInfo; };
    
    static constexpr auto value = glz::object(
        "ioInfo", &T::IoInfo,
        "metadata", &T::Metadata,
        "assetSpecificInfo", glz::custom<READ_H, WRITE_H>
    );
};

namespace fs = std::filesystem;

namespace assetlib
{

namespace 
{
constexpr std::string_view ASSET_COMBINED_FILE_MAGIC = "ASSETBFF";
constexpr u32 ASSET_COMBINED_FILE_MAGIC_LENGTH = (u32)ASSET_COMBINED_FILE_MAGIC.length();
static_assert(ASSET_COMBINED_FILE_MAGIC_LENGTH == 8);

constexpr u32 ASSET_CURRENT_VERSION = 1;

io::IoResult<AssetFile> unpackBaseAssetHeaderFromBuffer(std::string_view buffer)
{
    const auto result = glz::read_json<AssetFile>(buffer);
    if (!result.has_value())
        return std::unexpected(io::IoError{io::IoError::ErrorCode::GeneralError,
            glz::format_error(result.error(), buffer)});

    return *result;
}

io::IoResult<AssetFile> unpackBaseAssetHeader(const std::filesystem::path& headerPath)
{
    std::ifstream headerIn(headerPath, std::ios::ate | std::ios::binary);
    ASSETLIB_CHECK_RETURN_IO_ERROR(headerIn.good(), io::IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open header file: {}", headerPath.string())

    const isize headerSize = headerIn.tellg();
    headerIn.seekg(0, std::ios::beg);
    std::string buffer(headerSize, 0);
    headerIn.read(buffer.data(), headerSize);
    
    return unpackBaseAssetHeaderFromBuffer(buffer);
}

io::IoResult<std::string> getAssetFullHeaderString(const AssetFile& file)
{
    const auto result =  glz::write_json(file);
    if (!result.has_value())
        return std::unexpected(io::IoError{io::IoError::ErrorCode::GeneralError, glz::format_error(result.error())});
    
    return glz::prettify_json(*result);
}
io::IoResult<std::string> getAssetFullHeaderFormattedString(const AssetFile& file)
{
    const auto result =  getAssetFullHeaderString(file)
        .transform([](const std::string& header) {
            return glz::prettify_json(header);
        });

    return result;
}
}


namespace io
{
IoResult<void> saveAssetFile(const AssetFile& file, Span<const std::byte> binaryData)
{
    const auto& headerPath = file.IoInfo.HeaderFile;
    const auto& binaryPath = file.IoInfo.BinaryFile;
    ASSETLIB_CHECK_RETURN_IO_ERROR(!headerPath.empty() && !binaryPath.empty(), IoError::ErrorCode::FailedToCreate,
        "Assetlib: File paths are not set: header: {}, binary: {}", headerPath.string(), binaryPath.string())
    
    bool success = fs::exists(fs::path(headerPath).parent_path()) ||
        fs::create_directories(fs::path(headerPath).parent_path());
    ASSETLIB_CHECK_RETURN_IO_ERROR(success, IoError::ErrorCode::FailedToCreate,
        "Assetlib: Failed to create header directory: {}", headerPath.string())

    if (binaryPath != headerPath)
    {
        success =  fs::exists(fs::path(binaryPath).parent_path()) ||
            fs::create_directories(fs::path(binaryPath).parent_path());
        ASSETLIB_CHECK_RETURN_IO_ERROR(success, IoError::ErrorCode::FailedToCreate,
            "Assetlib: Failed to create binary directory: {}", binaryPath.string())
    }

    std::ofstream headerOut(headerPath, std::ios::out);
    ASSETLIB_CHECK_RETURN_IO_ERROR(headerOut.good(), IoError::ErrorCode::FailedToCreate,
        "Assetlib: Failed to create header file: {}", headerPath.string())
    std::ofstream binaryOut(binaryPath, std::ios::out | std::ios::binary);
    ASSETLIB_CHECK_RETURN_IO_ERROR(binaryOut.good(), IoError::ErrorCode::FailedToCreate,
        "Assetlib: Failed to create binary file: {}", binaryPath.string())

    const auto assetHeaderString = getAssetFullHeaderFormattedString(file);
    ASSETLIB_CHECK_RETURN_IO_ERROR(assetHeaderString.has_value(), IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to create header string: {} ({})", assetHeaderString.error().Message, headerPath.string())
    headerOut.write(assetHeaderString->c_str(), (isize)assetHeaderString->size());
    binaryOut.write((const char*)binaryData.data(), (isize)binaryData.size());

    return {};
}

IoResult<AssetFileAndBinary> loadAssetFile(const std::filesystem::path& headerPath)
{
    const auto file = loadAssetFileHeader(headerPath);
    if (!file.has_value())
        return std::unexpected(file.error());

    const auto binary = loadAssetFileBinaries(*file);
    if (!binary.has_value())
        return std::unexpected(binary.error());

    return AssetFileAndBinary{
        .File = *file,
        .Binary = *binary,
    };
}

IoResult<AssetFile> loadAssetFileHeader(const std::filesystem::path& headerPath)
{
    auto assetHeader = unpackBaseAssetHeader(headerPath);
    ASSETLIB_CHECK_RETURN_IO_ERROR(assetHeader.has_value(), IoError::ErrorCode::WrongFormat,
       "Assetlib: Failed to parse header file: {} ({})", assetHeader.error().Message, headerPath.string())

    return assetHeader;
}

IoResult<AssetBinary> loadAssetFileBinaries(const AssetFile& file)
{
    return loadAssetFileBinaries(file, 0, file.IoInfo.BinarySizeBytesCompressed);
}

IoResult<AssetBinary> loadAssetFileBinaries(const AssetFile& file, u64 offsetBytes, u64 sizeBytes)
{
    const fs::path binaryPath = file.IoInfo.BinaryFile;

    std::ifstream binaryIn(binaryPath, std::ios::binary | std::ios::ate);
    ASSETLIB_CHECK_RETURN_IO_ERROR(binaryIn.good(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open binary file: {}", binaryPath.string())

    const isize totalFileSize = binaryIn.tellg();
    ASSETLIB_CHECK_RETURN_IO_ERROR(totalFileSize - offsetBytes >= sizeBytes,
        IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed total file size is less then offset + size: {}", binaryPath.string())
        
    AssetBinary binaryData = {};
    binaryData.resize(sizeBytes);
    binaryIn.seekg((isize)offsetBytes, std::ios::beg);
    binaryIn.read((char *)binaryData.data(), (isize)sizeBytes);

    return binaryData;
}

IoResult<void> saveAssetFileCombined(const AssetFile& file, Span<const std::byte> binaryData)
{
    ASSETLIB_CHECK_RETURN_IO_ERROR(file.IoInfo.HeaderFile == file.IoInfo.BinaryFile, IoError::ErrorCode::FailedToCreate,
        "Assetlib: File paths for combined assets have to be equal")

    const fs::path path = file.IoInfo.HeaderFile;
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

    const isize binarySizeBytes = (isize)binaryData.size();
    out.write((const char*)&binarySizeBytes, sizeof binarySizeBytes);
    
    out.write(assetHeaderString->data(), headerSizeBytes);
    out.write((const char*)binaryData.data(), (isize)binaryData.size());

    return {};
}

IoResult<AssetFileAndBinary> loadAssetFileCombined(const std::filesystem::path& path)
{
    const auto file = loadAssetFileCombinedHeader(path);
    if (!file.has_value())
        return std::unexpected(file.error());
    
    const auto binary = loadAssetFileCombinedBinaries(*file);
    if (!binary.has_value())
        return std::unexpected(binary.error());

    return AssetFileAndBinary{
        .File = *file,
        .Binary = *binary,
    };
}

IoResult<AssetFile> loadAssetFileCombinedHeader(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    ASSETLIB_CHECK_RETURN_IO_ERROR(in.good(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open combined asset file: {}", path.string())

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
       "Assetlib: Failed to parse header file: {} ({})", assetFile.error().Message, path.string())

    assetFile->IoInfo.HeaderSizeBytes = headerSizeBytes;
    assetFile->IoInfo.BinarySizeBytesCompressed = binarySizeBytes;

    return assetFile;
}

IoResult<AssetBinary> loadAssetFileCombinedBinaries(const AssetFile& file)
{
    return loadAssetFileCombinedBinaries(file, 0, file.IoInfo.BinarySizeBytesCompressed);
}

IoResult<AssetBinary> loadAssetFileCombinedBinaries(const AssetFile& file, u64 offsetBytes, u64 sizeBytes)
{
    const isize headerSizeBytes = (isize)file.IoInfo.HeaderSizeBytes;
    const fs::path binaryPath = file.IoInfo.BinaryFile;

    std::ifstream binaryIn(binaryPath, std::ios::binary);
    ASSETLIB_CHECK_RETURN_IO_ERROR(binaryIn.good(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open binary file: {}", binaryPath.string())
    const isize totalFileSize = binaryIn.tellg();
    ASSETLIB_CHECK_RETURN_IO_ERROR(totalFileSize - offsetBytes - headerSizeBytes >= sizeBytes,
        IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed total file size is less then offset + size: {}", binaryPath.string())
    
    AssetBinary binaryData = {};
    binaryData.resize(sizeBytes);
    binaryIn.seekg((isize)(
        ASSET_COMBINED_FILE_MAGIC_LENGTH +
        sizeof file.Metadata.Version +
        sizeof file.IoInfo.HeaderSizeBytes +
        file.IoInfo.HeaderSizeBytes +
        offsetBytes), std::ios::beg);
    binaryIn.read((char *)binaryData.data(), (isize)sizeBytes);

    return binaryData;
}
}

}
