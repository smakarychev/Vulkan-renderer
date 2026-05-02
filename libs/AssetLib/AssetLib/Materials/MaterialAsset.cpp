#include "MaterialAsset.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

template <>
struct glz::meta<lux::assetlib::MaterialAlphaMode> : lux::assetlib::reflection::CamelCase
{
    using enum lux::assetlib::MaterialAlphaMode;
    static constexpr auto value = glz::enumerate(
        Opaque, Mask, Translucent
    );
};
template <> struct ::glz::meta<lux::assetlib::MaterialAsset> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib
{
namespace material
{
io::IoResult<MaterialAsset> readMaterial(const AssetMetadata& metadata)
{
    auto headerRead = readFileToString(metadata.Io.HeaderFile);
    ASSETLIB_CHECK_RETURN_IO_ERROR(headerRead.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read header file: {}", metadata.Io.HeaderFile.string())
    
    const auto result = glz::read_json<MaterialAsset>(*headerRead);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: {}", glz::format_error(result.error(), *headerRead))

    return *result;
}

io::IoResult<AssetPacked> pack(const MaterialAsset& material)
{
    auto header = glz::write_json(material);
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(header.error()))

    return AssetPacked{
        .Header = std::move(*header),
    };
}
}

}
