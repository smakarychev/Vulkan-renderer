#pragma once

#include "AssetLib/v3/Assetlibv3.h"

namespace lux::assetlib
{
class AssetResolvedPath
{
public:
    auto operator<=>(const AssetResolvedPath&) const = default;

    AssetResolvedPath() = default;

    AssetResolvedPath(std::string package, std::string assetName,
        std::optional<std::string> variant)
        : m_PackagePath(std::move(package)), m_AssetName(std::move(assetName)), m_Variant(std::move(variant))
    {
    }

    const std::string& GetPackagePath() const { return m_PackagePath; }
    const std::string& GetAssetName() const { return m_AssetName; }
    const std::optional<std::string>& GetVariant() const { return m_Variant; }

private:
    std::string m_PackagePath{};
    std::string m_AssetName{};
    std::optional<std::string> m_Variant{std::nullopt};
};

class IAssetPathResolver : public IAssetSystemIdentifiable
{
public:
    LUX_RTTI(IAssetPathResolver, IAssetSystemIdentifiable)

    virtual std::optional<AssetResolvedPath> Resolve(const std::string& path) const = 0;
    virtual void InvalidateCache() = 0;
    virtual void InvalidateCache(const AssetResolvedPath& path) = 0;
};
}

namespace std
{
template <>
struct hash<lux::assetlib::AssetResolvedPath>
{
    size_t operator()(const lux::assetlib::AssetResolvedPath& path) const noexcept
    {
        u64 hash = std::hash<std::string>{}(path.GetPackagePath());
        Hash::combine(hash, std::hash<std::string>{}(path.GetAssetName()));
        Hash::combine(hash, std::hash<std::optional<std::string>>{}(path.GetVariant()));

        return hash;
    }
};
}
