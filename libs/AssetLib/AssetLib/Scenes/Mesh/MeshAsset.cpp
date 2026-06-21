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
template <> struct ::glz::meta<lux::assetlib::MeshAttribute> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::MeshPrimitiveBlendShape> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::MeshPrimitive> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::MeshAsset> : lux::assetlib::reflection::CamelCase {};

namespace
{
const lux::assetlib::MeshAttribute* findAttribute(std::string_view name,
    const std::vector<lux::assetlib::MeshAttribute>& attributes)
{
    auto it = std::ranges::find_if(attributes, [&](auto& attribute) { return attribute.Name == name; });
    
    return it == attributes.end() ? nullptr : &*it;
}
}

const lux::assetlib::MeshAttribute* lux::assetlib::MeshPrimitiveBlendShape::FindAttribute(std::string_view name) const
{
    return findAttribute(name, Attributes);
}

const lux::assetlib::MeshAttribute* lux::assetlib::MeshPrimitive::FindAttribute(std::string_view name) const
{
    return findAttribute(name, Attributes);
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




