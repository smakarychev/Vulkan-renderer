#include "rendererpch.h"
#include "AssetIdResolver.h"

#include <shared_mutex>

namespace lux
{
const AssetIdResolver::AssetInfo* AssetIdResolver::Resolve(assetlib::AssetId id) const
{
    std::shared_lock lock(m_Mutex);
    auto it = m_AssetInfos.find(id);
    if (it == m_AssetInfos.end())
        return nullptr;
    
    return &it->second;
}

void AssetIdResolver::RegisterId(assetlib::AssetId id, const AssetInfo& assetInfo)
{
    std::unique_lock lock(m_Mutex);
    if (m_AssetInfos.contains(id))
        return;
    
    LUX_LOG_TRACE("Asset id registered: {} {} {}", id, assetInfo.AssetType, assetInfo.Path.ToString());
    m_AssetInfos[id] = assetInfo;
}
}
