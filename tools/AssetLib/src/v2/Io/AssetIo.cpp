#include "AssetIo.h"

#include "v2/Reflection/AssetLibReflectionUtility.inl"

template <> struct ::glz::meta<lux::assetlib::AssetFileIoInfo> : lux::assetlib::reflection::CamelCase {}; 
template <> struct ::glz::meta<lux::assetlib::AssetMetadata> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::AssetFile> : lux::assetlib::reflection::CamelCase {
    using T = lux::assetlib::AssetFile;
    static constexpr auto READ_H = [](T& file, glz::raw_json&& input) { file.AssetSpecificInfo = std::move(input.str); };
    static constexpr auto WRITE_H = [](auto& file) -> glz::raw_json_view { return file.AssetSpecificInfo; };
    
    static constexpr auto value = glz::object(
        "ioInfo", &T::IoInfo,
        "metadata", &T::Metadata,
        "assetSpecificInfo", glz::custom<READ_H, WRITE_H>
    );
};


namespace lux::assetlib::io
{
IoResult<AssetFile> unpackBaseAssetHeaderFromBuffer(std::string_view buffer)
{
    const auto result = glz::read_json<AssetFile>(buffer);
    if (!result.has_value())
        return std::unexpected(io::IoError{
            io::IoError::ErrorCode::GeneralError,
            glz::format_error(result.error(), buffer)
        });

    return *result;
}

IoResult<AssetFile> unpackBaseAssetHeader(const std::filesystem::path& headerPath)
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

IoResult<std::string> getAssetFullHeaderString(const AssetFile& file)
{
    const auto result = glz::write_json(file);
    if (!result.has_value())
        return std::unexpected(io::IoError{io::IoError::ErrorCode::GeneralError, glz::format_error(result.error())});

    return glz::prettify_json(*result);
}

IoResult<std::string> getAssetFullHeaderFormattedString(const AssetFile& file)
{
    const auto result = getAssetFullHeaderString(file)
        .transform([](const std::string& header)
        {
            return glz::prettify_json(header);
        });

    return result;
}
}
