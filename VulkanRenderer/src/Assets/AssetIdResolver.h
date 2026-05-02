#pragma once

#include <AssetLib/Assetlib.h>

#include <shared_mutex>

namespace lux
{
class AssetIdResolver
{
public:
    struct AssetInfo
    {
        std::filesystem::path Path{};
        std::filesystem::path MetaPath{};
        assetlib::AssetType AssetType{};
    };
public:
    const AssetInfo* Resolve(assetlib::AssetId id) const;
    assetlib::AssetId ResolveMetaPath(const std::filesystem::path& path) const;
    void RegisterId(assetlib::AssetId id, const AssetInfo& assetInfo);
private:
    std::unordered_map<assetlib::AssetId, AssetInfo> m_AssetInfos;
    std::unordered_map<std::filesystem::path, assetlib::AssetId> m_AssetPathToId;
    mutable std::shared_mutex m_Mutex;
};
}
