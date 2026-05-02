#include "ShaderMeta.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

template <> struct glz::meta<lux::assetlib::ShaderMeta> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib
{
io::IoResult<ShaderMeta> shader::readMeta(const std::filesystem::path& path)
{
    DEFINE_BASIC_METADATA_READ(ShaderMeta, meta, path)
    return meta;
}

io::IoResult<std::string> shader::packMeta(const ShaderMeta& shaderMeta)
{
    DEFINE_BASIC_METADATA_PACK(ShaderMeta, meta, shaderMeta)
    return *meta;
}

AssetTypeMetadata shader::getTypeMetadata()
{
    static constexpr u32 SHADER_ASSET_VERSION = 1;
    
    return {
        .Type = ASSET_TYPE,
        .Name = "shader",
        .Version = SHADER_ASSET_VERSION,
    };
}
}
