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
using AssetSlotMap = SlotMapType<Asset, AssetHandle<Asset>>;

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
}
