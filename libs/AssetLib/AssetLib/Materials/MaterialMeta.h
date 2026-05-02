#pragma once

#include <AssetLib/Io/AssetIo.h>

namespace lux::assetlib
{
struct MaterialMeta
{
    AssetMetadata Metadata{};
};

namespace material
{
io::IoResult<MaterialMeta> readMeta(const std::filesystem::path& path);
io::IoResult<std::string> packMeta(const MaterialMeta& materialMeta);

static constexpr AssetType ASSET_TYPE = "707d8c4d-3447-48c8-a3a3-4d911aa8a0eb"_guid;
AssetTypeMetadata getTypeMetadata();
}
}
