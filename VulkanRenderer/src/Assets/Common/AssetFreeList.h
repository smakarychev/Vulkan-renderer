#pragma once
#include "Assets/AssetHandle.h"

#include <AssetLib/AssetId.h>
#include <CoreLib/Containers/FreeList.h>

namespace lux
{
template <typename Asset>
class AssetFreeList
{
public:
    using Handle = AssetHandle<Asset>;

    constexpr Handle Insert(Asset&& asset);
    constexpr void Erase(Handle handle);

    constexpr const Asset& operator[](u32 index) const;
    constexpr Asset& operator[](u32 index);
private:
    FreeList<Asset> m_Assets;
};

template <typename Asset>
constexpr AssetFreeList<Asset>::Handle AssetFreeList<Asset>::Insert(Asset&& asset)
{
    const u32 index = m_Assets.insert(std::move(asset));
    const Handle handle(index, 0);

    return handle;
}

template <typename Asset>
constexpr void AssetFreeList<Asset>::Erase(Handle handle)
{
    m_Assets.erase(handle.Index());
}

template <typename Asset>
constexpr const Asset& AssetFreeList<Asset>::operator[](u32 index) const
{
    return m_Assets[index];
}

template <typename Asset>
constexpr Asset& AssetFreeList<Asset>::operator[](u32 index)
{
    return const_cast<Asset&>(const_cast<const AssetFreeList&>(*this)[index]);
}
}
