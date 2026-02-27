#include "MaterialAsset.h"

#include "v2/Reflection/AssetLibReflectionUtility.inl"

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
io::IoResult<MaterialAsset> readMaterial(const AssetFile& assetFile)
{
    const auto result = glz::read_json<MaterialAsset>(assetFile.AssetSpecificInfo);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: {}", glz::format_error(result.error(), assetFile.AssetSpecificInfo))

    return *result;
}

io::IoResult<AssetPacked> pack(const AssetFile& material)
{
    auto header = glz::write_json(material);
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(header.error()))

    return AssetPacked{
        .Metadata = getMetadata(),
        .AssetSpecificInfo = std::move(*header),
    };
}

AssetMetadata getMetadata()
{
    static constexpr u32 MATERIAL_ASSET_VERSION = 1;
    
    return {
        .Type = "707d8c4d-3447-48c8-a3a3-4d911aa8a0eb"_guid,
        .TypeName = "material",
        .Version = MATERIAL_ASSET_VERSION,
    };
}
}

}
