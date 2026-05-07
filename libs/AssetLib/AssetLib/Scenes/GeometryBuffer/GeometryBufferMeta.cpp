#include "GeometryBufferMeta.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

template <> struct glz::meta<lux::assetlib::GeometryBufferMeta> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib
{
io::IoResult<GeometryBufferMeta> sceneGeometry::readMeta(const std::filesystem::path& path)
{
    DEFINE_BASIC_METADATA_READ(GeometryBufferMeta, meta, path)
    return meta;
}

io::IoResult<std::string> sceneGeometry::packMeta(const GeometryBufferMeta& geometryMeta)
{
    DEFINE_BASIC_METADATA_PACK(GeometryBufferMeta, meta, geometryMeta)
    return *meta;
}

AssetTypeMetadata sceneGeometry::getTypeMetadata()
{
    static constexpr u32 GEOMETRY_ASSET_VERSION = 1;
    
    return {
        .Type = ASSET_TYPE,
        .Name = "scene/geometry",
        .Version = GEOMETRY_ASSET_VERSION,
    };
}
}
