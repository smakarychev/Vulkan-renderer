#include "rendererpch.h"
#include "AssetIdResolver.h"

namespace lux
{
const AssetIdResolver::AssetInfo* AssetIdResolver::Resolve(assetlib::AssetId id) const
{
    auto it = m_AssetInfos.find(id);
    if (it == m_AssetInfos.end())
        return nullptr;
    
    return &it->second;
}

void AssetIdResolver::RegisterId(assetlib::AssetId id, const AssetInfo& assetInfo)
{
    m_AssetInfos[id] = assetInfo;
}
}
