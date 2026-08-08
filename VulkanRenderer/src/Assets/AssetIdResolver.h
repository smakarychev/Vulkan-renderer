#pragma once

#include "AssetPath.h"

#include <AssetLib/Assetlib.h>

#include <shared_mutex>

namespace lux
{
class AssetIdResolver
{
public:
    struct AssetInfo
    {
        AssetPath Path{};
        std::filesystem::path MetaPath{};
        assetlib::AssetType AssetType{};
    };
public:
    const AssetInfo* Resolve(assetlib::AssetId id) const;
    void RegisterId(assetlib::AssetId id, const AssetInfo& assetInfo);
private:
    std::unordered_map<assetlib::AssetId, AssetInfo> m_AssetInfos;
    mutable std::shared_mutex m_Mutex;
};
}
