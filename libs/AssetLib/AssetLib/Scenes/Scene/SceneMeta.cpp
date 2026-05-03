#include "SceneMeta.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

template <> struct glz::meta<lux::assetlib::SceneMeta> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib
{
io::IoResult<SceneMeta> scene::readMeta(const std::filesystem::path& path)
{
    DEFINE_BASIC_METADATA_READ(SceneMeta, meta, path)
    return meta;
}

io::IoResult<std::string> scene::packMeta(const SceneMeta& sceneMeta)
{
    DEFINE_BASIC_METADATA_PACK(SceneMeta, meta, sceneMeta)
    return *meta;
}

AssetTypeMetadata scene::getTypeMetadata()
{   
    static constexpr u32 SCENE_ASSET_VERSION = 1;
    
    return {
        .Type = ASSET_TYPE,
        .Name = "scene",
        .Version = SCENE_ASSET_VERSION,
    };
}
}
