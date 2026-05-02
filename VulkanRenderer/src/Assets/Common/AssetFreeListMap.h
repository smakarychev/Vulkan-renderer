#pragma once
#include "Assets/AssetHandle.h"

#include <CoreLib/Containers/FreeList.h>

namespace lux
{
template <typename Asset>
class AssetFreeListMap
{
public:
    using Handle = AssetHandle<Asset>;
    constexpr Handle Find(assetlib::AssetId id) const;
    constexpr assetlib::AssetId Find(Handle handle) const;

    constexpr Handle Add(Asset&& asset, assetlib::AssetId id);
    constexpr void Erase(Handle handle, assetlib::AssetId id);

    constexpr const Asset& operator[](u32 index) const;
    constexpr Asset& operator[](u32 index);
    
    constexpr auto begin() const { return m_AssetsMap.begin(); }
    constexpr auto end() const { return m_AssetsMap.end(); }
private:
    FreeList<Asset> m_Assets;
    std::unordered_map<assetlib::AssetId, AssetHandle<Asset>> m_AssetsMap;
    std::vector<assetlib::AssetId> m_HandlesToIds;
};

template <typename Asset>
constexpr AssetFreeListMap<Asset>::Handle AssetFreeListMap<Asset>::Find(assetlib::AssetId id) const
{
    auto it = m_AssetsMap.find(id);
    if (it == m_AssetsMap.end())
        return {};
    
    return it->second;
}

template <typename Asset>
constexpr assetlib::AssetId AssetFreeListMap<Asset>::Find(Handle handle) const
{
    if (handle.Index() >= m_HandlesToIds.size())
        return assetlib::AssetId::CreateEmpty();

    const assetlib::AssetId id = m_HandlesToIds[handle.Index()];
    if (!id.HasValue())
        return assetlib::AssetId::CreateEmpty();

    return id;
}

template <typename Asset>
constexpr AssetFreeListMap<Asset>::Handle AssetFreeListMap<Asset>::Add(Asset&& asset, assetlib::AssetId id)
{
    const u32 index = m_Assets.insert(std::move(asset));
    const Handle handle(index, 0);
    m_AssetsMap[id] = handle;
    if (m_HandlesToIds.size() <= handle.Index())
        m_HandlesToIds.resize(handle.Index() + 1);
    m_HandlesToIds[handle.Index()] = id;

    return handle;
}

template <typename Asset>
constexpr void AssetFreeListMap<Asset>::Erase(Handle handle, assetlib::AssetId id)
{
    m_AssetsMap.erase(id);
    m_HandlesToIds[handle.Index()] = assetlib::AssetId::CreateEmpty();
    m_Assets.erase(handle.Index());
}

template <typename Asset>
constexpr const Asset& AssetFreeListMap<Asset>::operator[](u32 index) const
{
    return m_Assets[index];
}

template <typename Asset>
constexpr Asset& AssetFreeListMap<Asset>::operator[](u32 index)
{
    return const_cast<Asset&>(const_cast<const AssetFreeListMap&>(*this)[index]);
}
}
