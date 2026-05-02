#include "MaterialMeta.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

template <> struct glz::meta<lux::assetlib::MaterialMeta> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib
{
io::IoResult<MaterialMeta> material::readMeta(const std::filesystem::path& path)
{
    DEFINE_BASIC_METADATA_READ(MaterialMeta, meta, path)
    return meta;
}

io::IoResult<std::string> material::packMeta(const MaterialMeta& materialMeta)
{
    DEFINE_BASIC_METADATA_PACK(MaterialMeta, meta, materialMeta)
    return *meta;
}

AssetTypeMetadata material::getTypeMetadata()
{
    static constexpr u32 MATERIAL_ASSET_VERSION = 1;
    
    return {
        .Type = ASSET_TYPE,
        .Name = "material",
        .Version = MATERIAL_ASSET_VERSION,
    };
}
}
