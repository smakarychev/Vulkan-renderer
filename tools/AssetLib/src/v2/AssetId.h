#pragma once

#include "types.h"
#include "Reflection/AssetLibReflection.h"

#include <format>

namespace assetlib
{
class AssetId
{
public:
    AssetId();
    AssetId(reflection::MakeReflectable) {}
    constexpr AssetId(u64 id) : m_Id(id) {}
    AssetId(const AssetId&) = default;
    AssetId(AssetId&&) noexcept = default;
    AssetId& operator=(const AssetId&) = default;
    AssetId& operator=(AssetId&&) noexcept = default;
    ~AssetId() = default;

    void Generate();
    constexpr u64 AsU64() const { return m_Id; }
    constexpr void FromU64(u64 value) { m_Id = value; }
private:
    u64 m_Id{};
};
}

namespace std
{
template <>
struct hash<assetlib::AssetId>
{
    usize operator()(const assetlib::AssetId id) const noexcept
    {
        return std::hash<u64>{}(id.AsU64());
    }
};
    
template <>
struct formatter<assetlib::AssetId> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(assetlib::AssetId id, format_context& ctx) const
    {
        return format_to(ctx.out(), "{}", id.AsU64());
    }
};
}
