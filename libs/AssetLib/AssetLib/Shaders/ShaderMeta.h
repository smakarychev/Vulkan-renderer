#pragma once

#include <AssetLib/Io/AssetIo.h>

namespace lux::assetlib
{
struct ShaderMeta
{
    AssetMetadata Metadata{};
    std::string VariantName{};
};

namespace shader
{
io::IoResult<ShaderMeta> readMeta(const std::filesystem::path& path);
io::IoResult<std::string> packMeta(const ShaderMeta& shaderMeta);

static constexpr AssetType ASSET_TYPE = "fb8f866d-d436-48d3-beef-2cd3dca6e0c8"_guid;
AssetTypeMetadata getTypeMetadata();
}
}
