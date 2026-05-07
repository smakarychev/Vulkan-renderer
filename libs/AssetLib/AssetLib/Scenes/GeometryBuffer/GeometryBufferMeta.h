#pragma once

#include <AssetLib/Io/AssetIo.h>

namespace lux::assetlib
{
struct GeometryBufferMeta
{
    AssetMetadata Metadata{};
    u64 SourceHash{};
    std::string SourceUri{};
};

namespace sceneGeometry
{
io::IoResult<GeometryBufferMeta> readMeta(const std::filesystem::path& path);
io::IoResult<std::string> packMeta(const GeometryBufferMeta& geometryMeta);

static constexpr AssetType ASSET_TYPE = "b3df8a4a-6b63-4084-a089-ba3f1b1f93d9"_guid;
AssetTypeMetadata getTypeMetadata();
}
}
