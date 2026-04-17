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
    constexpr Handle Find(const std::filesystem::path& path) const;
    constexpr const std::filesystem::path* Find(Handle handle) const;

    constexpr Handle Add(Asset&& asset, const std::filesystem::path& path);
    constexpr void Erase(Handle handle, const std::filesystem::path& path);

    constexpr const Asset& operator[](Handle handle) const;
    constexpr Asset& operator[](Handle handle);
    
    constexpr auto begin() const { return m_AssetsMap.begin(); }
    constexpr auto end() const { return m_AssetsMap.end(); }
private:
    SlotMapType<Asset, Handle> m_Assets;
    std::unordered_map<std::filesystem::path, AssetHandle<Asset>> m_AssetsMap;
    std::vector<std::filesystem::path> m_HandlesToPaths;
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
constexpr AssetSlotMap<Asset>::Handle AssetSlotMap<Asset>::Find(const std::filesystem::path& path) const
{
    auto it = m_AssetsMap.find(path);
    if (it == m_AssetsMap.end())
        return {};
    
    return it->second;
}

template <typename Asset>
constexpr const std::filesystem::path* AssetSlotMap<Asset>::Find(Handle handle) const
{
    if (handle.Index() >= m_HandlesToPaths.size())
        return nullptr;

    const auto& path = m_HandlesToPaths[handle.Index()];
    if (path.empty())
        return nullptr;

    return &path;
}

template <typename Asset>
constexpr AssetSlotMap<Asset>::Handle AssetSlotMap<Asset>::Add(Asset&& asset, const std::filesystem::path& path)
{
    const Handle handle = m_Assets.insert(std::move(asset));
    m_AssetsMap[path] = handle;
    if (m_HandlesToPaths.size() <= handle.Index())
        m_HandlesToPaths.resize(handle.Index() + 1);
    m_HandlesToPaths[handle.Index()] = path;

    return handle;
}

template <typename Asset>
constexpr void AssetSlotMap<Asset>::Erase(Handle handle, const std::filesystem::path& path)
{
    m_AssetsMap.erase(path);
    m_HandlesToPaths[handle.Index()] = std::filesystem::path{};
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
