#pragma once

#include <AssetLib/Io/AssetIo.h>

namespace lux::assetlib
{
struct MeshMeta
{
    AssetMetadata Metadata{};
};

namespace sceneMesh
{
io::IoResult<MeshMeta> readMeta(const std::filesystem::path& path);
io::IoResult<std::string> packMeta(const MeshMeta& meshMeta);

static constexpr AssetType ASSET_TYPE = "5d6581a7-6b24-4471-82f1-cec792dec1ba"_guid;
AssetTypeMetadata getTypeMetadata();
}
}