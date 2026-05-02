#include "AssetIo.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

namespace lux::assetlib::io
{
struct MetadataEnvelope
{
    AssetMetadata Metadata{};
};
}
template <> struct ::glz::meta<lux::assetlib::io::MetadataEnvelope> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib::io
{
IoResult<AssetMetadata> readBaseAssetMetadata(const std::filesystem::path& path)
{
    auto read = readFileToString(path);
    ASSETLIB_CHECK_RETURN_IO_ERROR(read.has_value(), IoError::ErrorCode::FailedToOpen,
        "Assetlib: Failed to open metadata file: {}", path.string())
    
    MetadataEnvelope assetMetadata;
    if (const auto error = glz::read<glz::opts{.error_on_unknown_keys = false}>(assetMetadata, *read))
        return std::unexpected(IoError{
            IoError::ErrorCode::GeneralError,
            glz::format_error(error, *read)
        });

    return assetMetadata.Metadata;
}

std::string getAssetHeaderFormatted(std::string_view header)
{
    return glz::prettify_json(header);
}
}
