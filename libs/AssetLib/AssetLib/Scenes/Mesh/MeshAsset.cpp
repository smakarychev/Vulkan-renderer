#include "MeshAsset.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

template <>
struct glz::meta<lux::assetlib::MeshPrimitiveTextureFilter> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::MeshPrimitiveTextureFilter;
    static constexpr auto value = glz::enumerate(Linear, Nearest);
};
template <> struct ::glz::meta<lux::assetlib::MeshPrimitiveTextureSample> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::MeshPrimitiveMaterial> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::MeshPrimitive::Attribute> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::MeshPrimitive> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::MeshAsset> : lux::assetlib::reflection::CamelCase {};


const lux::assetlib::MeshPrimitive::Attribute* lux::assetlib::MeshPrimitive::FindAttribute(std::string_view name) const
{
    auto it = std::ranges::find_if(Attributes, [&](auto& attribute) { return attribute.Name == name; });
    
    return it == Attributes.end() ? nullptr : &*it;
}

namespace lux::assetlib::sceneMesh
{
io::IoResult<MeshAsset> readMesh(const AssetMetadata& metadata)
{
    DEFINE_BASIC_HEADER_READ(MeshAsset, result, metadata)
    return *result;
}

io::IoResult<AssetPacked> pack(const MeshAsset& mesh)
{
    auto header = glz::write_json(mesh);
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(header.error()))

    return AssetPacked{
        .Header = std::move(*header),
    };
}
}




