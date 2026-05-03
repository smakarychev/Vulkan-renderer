#pragma once

#include <AssetLib/Io/AssetIo.h>

namespace lux::assetlib
{
struct SceneMeta
{
    AssetMetadata Metadata{};
};

namespace scene
{
io::IoResult<SceneMeta> readMeta(const std::filesystem::path& path);
io::IoResult<std::string> packMeta(const SceneMeta& sceneMeta);

static constexpr AssetType ASSET_TYPE = "7b331c56-5ad1-4269-8737-7a6c0a9f46da"_guid;
AssetTypeMetadata getTypeMetadata();
}
}
