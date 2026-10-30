#pragma once
#include "AssetMetadataDatabase.h"
#include "AssetPathResolver.h"

namespace lux::assetlib
{
class AssetMetadataDefaultDatabase final : public IAssetMetadataDatabase
{
public:
    LUX_RTTI(AssetMetadataDefaultDatabase, IAssetMetadataDatabase)
    DEFINE_ASSETLIB_IDENTIFIABLE(AssetMetadataDefaultDatabase, "10e61810-b159-40a1-ad63-d45a114f66f0"_guid)
    
    AssetMetadataDefaultDatabase(const std::shared_ptr<IAssetPathResolver>& resolver);
    
    std::optional<AssetPackageMetadata2> Find(const AssetResolvedPath& resolvedPath) const override;
    std::optional<AssetPackageMetadata2> Find(const AssetId& packageId) const override;

    void ScanDirectory(const std::filesystem::path& directory) const;
    void UpdateMetadata(const std::filesystem::path& metadataPath) const;
private:
    void AddToCaches(AssetResolvedPath&& resolvedPath, AssetPackageMetadata2&& metadata) const;
    void ReplaceCaches(AssetResolvedPath&& resolvedPath, AssetPackageMetadata2&& metadata) const;
private:
    mutable std::mutex m_Mutex;

    std::shared_ptr<IAssetPathResolver> m_Resolver;
    mutable std::unordered_map<AssetId, AssetPackageMetadata2> m_Cache;
    mutable std::unordered_map<AssetResolvedPath, AssetPackageMetadata2*> m_PathLookup;
};
}
