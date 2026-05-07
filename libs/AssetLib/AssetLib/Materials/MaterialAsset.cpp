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
    DEFINE_BASIC_HEADER_READ(MaterialAsset, result, metadata)
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
