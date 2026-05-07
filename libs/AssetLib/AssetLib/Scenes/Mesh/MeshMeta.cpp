#include "MeshMeta.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

template <> struct glz::meta<lux::assetlib::MeshMeta> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib
{
io::IoResult<MeshMeta> sceneMesh::readMeta(const std::filesystem::path& path)
{
    DEFINE_BASIC_METADATA_READ(MeshMeta, meta, path)
    return meta;
}

io::IoResult<std::string> sceneMesh::packMeta(const MeshMeta& meshMeta)
{
    DEFINE_BASIC_METADATA_PACK(MeshMeta, meta, meshMeta)
    return *meta;
}

AssetTypeMetadata sceneMesh::getTypeMetadata()
{ 
    static constexpr u32 MESH_ASSET_VERSION = 1;
    
    return {
        .Type = ASSET_TYPE,
        .Name = "scene/mesh",
        .Version = MESH_ASSET_VERSION,
    };
}
}
