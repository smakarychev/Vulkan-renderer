#pragma once
#include "v2/AssetLibV2.h"

namespace lux
{
class AssetIdResolver
{
public:
    struct AssetInfo
    {
        std::filesystem::path Path{};
        assetlib::AssetType AssetType{};
    };
public:
    const AssetInfo* Resolve(assetlib::AssetId id) const;
    void RegisterId(assetlib::AssetId id, const AssetInfo& assetInfo);
private:
    std::unordered_map<assetlib::AssetId, AssetInfo> m_AssetInfos;
};
}
