#include "SceneAsset.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>

#include <CoreLib/Utils/FileUtils.h>


template <>
struct glz::meta<lux::assetlib::SceneAssetCameraType> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::SceneAssetCameraType;
    static constexpr auto value = glz::enumerate(Perspective, Orthographic);
};
template <> struct ::glz::meta<lux::assetlib::SceneAssetCamera::PerspectiveData>
    : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetCamera::OrthographicData>
    : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetCamera> : lux::assetlib::reflection::CamelCase {};
template <>
struct glz::meta<lux::assetlib::SceneAssetLightType> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::SceneAssetLightType;
    static constexpr auto value = glz::enumerate(Directional, Point, Spot);
};
template <> struct ::glz::meta<lux::assetlib::SceneAssetLight> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetMeshlet> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetSkin> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetNode> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetSubscene> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAsset> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib::scene
{
io::IoResult<SceneAsset> readScene(const AssetMetadata& metadata)
{
    DEFINE_BASIC_HEADER_READ(SceneAsset, result, metadata)
    return *result;
}

io::IoResult<AssetPacked> pack(const SceneAsset& scene)
{
    auto packed = glz::write_json(scene);
    ASSETLIB_CHECK_RETURN_IO_ERROR(packed.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(packed.error()))
    
    return AssetPacked{
        .Header = std::move(*packed),
    };
}
}
