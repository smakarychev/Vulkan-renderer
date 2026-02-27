#pragma once
#include "Assets/AssetHandle.h"
#include "Containers/FreeList.h"

namespace lux
{
template <typename Asset>
class AssetFreeListMap
{
public:
    using Handle = AssetHandle<Asset>;
    constexpr Handle Find(const std::filesystem::path& path) const;
    constexpr const std::filesystem::path* Find(Handle handle) const;

    constexpr Handle Add(Asset&& asset, const std::filesystem::path& path);
    constexpr void Erase(Handle handle, const std::filesystem::path& path);

    constexpr const Asset& operator[](u32 index) const;
    constexpr Asset& operator[](u32 index);
    
    constexpr auto begin() const { return m_AssetsMap.begin(); }
    constexpr auto end() const { return m_AssetsMap.end(); }
private:
    FreeList<Asset> m_Assets;
    std::unordered_map<std::filesystem::path, AssetHandle<Asset>> m_AssetsMap;
    std::vector<std::filesystem::path> m_HandlesToPaths;
};

template <typename Asset>
constexpr AssetFreeListMap<Asset>::Handle AssetFreeListMap<Asset>::Find(const std::filesystem::path& path) const
{
    auto it = m_AssetsMap.find(path);
    if (it == m_AssetsMap.end())
        return {};
    
    return it->second;
}

template <typename Asset>
constexpr const std::filesystem::path* AssetFreeListMap<Asset>::Find(Handle handle) const
{
    if (handle.Index() >= m_HandlesToPaths.size())
        return nullptr;

    const auto& path = m_HandlesToPaths[handle.Index()];
    if (path.empty())
        return nullptr;

    return &path;
}

template <typename Asset>
constexpr AssetFreeListMap<Asset>::Handle AssetFreeListMap<Asset>::Add(Asset&& asset, const std::filesystem::path& path)
{
    const u32 index = m_Assets.insert(std::move(asset));
    const Handle handle(index, 0);
    m_AssetsMap.emplace(path, handle);
    if (m_HandlesToPaths.size() <= handle.Index())
        m_HandlesToPaths.resize(handle.Index() + 1);
    m_HandlesToPaths[handle.Index()] = path;

    return handle;
}

template <typename Asset>
constexpr void AssetFreeListMap<Asset>::Erase(Handle handle, const std::filesystem::path& path)
{
    m_AssetsMap.erase(path);
    m_HandlesToPaths[handle.Index()] = std::filesystem::path{};
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
