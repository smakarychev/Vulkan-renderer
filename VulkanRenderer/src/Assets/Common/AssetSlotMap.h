#pragma once
#include "Assets/AssetHandle.h"

#include <CoreLib/Containers/SlotMapType.h>

namespace lux
{
template <typename Asset>
struct lux::SparseSetGenerationTraits<AssetHandle<Asset>>
{
    using Base = SparseSetGenerationTraits<u32>;
    using Handle = AssetHandle<Asset>;
    static constexpr std::pair<u32, u32> Decompose(const Handle& val);
    static constexpr Handle Compose(u32 generation, u32 value);
};


template <typename Asset>
class AssetSlotMap
{
public:
    using Handle = AssetHandle<Asset>;
    constexpr Handle Find(assetlib::AssetId id) const;
    constexpr assetlib::AssetId Find(Handle handle) const;

    constexpr Handle Add(Asset&& asset, assetlib::AssetId id);
    constexpr void Erase(Handle handle, assetlib::AssetId id);

    constexpr const Asset& operator[](Handle handle) const;
    constexpr Asset& operator[](Handle handle);
    
    constexpr auto begin() const { return m_AssetsMap.begin(); }
    constexpr auto end() const { return m_AssetsMap.end(); }
private:
    SlotMapType<Asset, Handle> m_Assets;
    std::unordered_map<assetlib::AssetId, AssetHandle<Asset>> m_AssetsMap;
    std::vector<assetlib::AssetId> m_HandlesToIds;
};

template <typename Asset>
constexpr std::pair<u32, u32> SparseSetGenerationTraits<AssetHandle<Asset>>::Decompose(const Handle& val)
{
    return {val.Version(), val.Index()};
}

template <typename Asset>
constexpr SparseSetGenerationTraits<AssetHandle<Asset>>::Handle 
    SparseSetGenerationTraits<AssetHandle<Asset>>::Compose(u32 generation, u32 value)
{
    return Handle(value, generation);
}

template <typename Asset>
constexpr AssetSlotMap<Asset>::Handle AssetSlotMap<Asset>::Find(assetlib::AssetId id) const
{
    auto it = m_AssetsMap.find(id);
    if (it == m_AssetsMap.end())
        return {};
    
    return it->second;
}

template <typename Asset>
constexpr assetlib::AssetId AssetSlotMap<Asset>::Find(Handle handle) const
{
    if (handle.Index() >= m_HandlesToIds.size())
        return assetlib::AssetId::CreateEmpty();

    const assetlib::AssetId id = m_HandlesToIds[handle.Index()];
    if (!id.HasValue())
        return assetlib::AssetId::CreateEmpty();

    return id;
}

template <typename Asset>
constexpr AssetSlotMap<Asset>::Handle AssetSlotMap<Asset>::Add(Asset&& asset, assetlib::AssetId id)
{
    const Handle handle = m_Assets.insert(std::move(asset));
    m_AssetsMap[id] = handle;
    if (m_HandlesToIds.size() <= handle.Index())
        m_HandlesToIds.resize(handle.Index() + 1);
    m_HandlesToIds[handle.Index()] = id;

    return handle;
}

template <typename Asset>
constexpr void AssetSlotMap<Asset>::Erase(Handle handle, assetlib::AssetId id)
{
    m_AssetsMap.erase(id);
    m_HandlesToIds[handle.Index()] = assetlib::AssetId::CreateEmpty();
    m_Assets.erase(handle);
}

template <typename Asset>
constexpr const Asset& AssetSlotMap<Asset>::operator[](Handle handle) const
{
    return m_Assets[handle];
}

template <typename Asset>
constexpr Asset& AssetSlotMap<Asset>::operator[](Handle handle)
{
    return const_cast<Asset&>(const_cast<const AssetSlotMap&>(*this)[handle]);
}
}
