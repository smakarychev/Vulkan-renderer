#pragma once

#include "AssetPathResolver.h"

namespace lux::assetlib
{
class AssetPathDefaultResolver final : public IAssetPathResolver
{
public:
    LUX_RTTI(AssetPathDefaultResolver, IAssetPathResolver)
    DEFINE_ASSETLIB_IDENTIFIABLE(AssetPathDefaultResolver, "c8c9e614-7ed8-45ce-afc4-62edae63a591"_guid)

    std::optional<AssetResolvedPath> Resolve(const std::string& path) const override;
    void InvalidateCache() override;
    void InvalidateCache(const AssetResolvedPath& path) override;
    
    void AddProtocolResolution(const std::string& protocol, const std::filesystem::path& path);
    void AddSearchPath(const std::filesystem::path& path);
private:
    struct Decomposed
    {
        std::string Package{};
        std::string AssetName{};
        std::optional<std::string> Variant{std::nullopt};
    };
private:
    std::optional<std::string> GetCanonicalPackagePath(const std::string& path) const;
    std::optional<std::string> FindPackagePath(const std::string& path) const;
    std::optional<Decomposed> Decompose(const std::string& path) const;
    std::optional<std::string> Preprocess(const std::string& path) const;
    void PostprocessInplace(std::string& path) const;
private:
    mutable std::mutex m_Mutex;
    mutable std::unordered_map<std::string, AssetResolvedPath> m_ResolvedPathCache{};
    mutable std::unordered_map<AssetResolvedPath, std::vector<std::string>> m_InverseLookupCache{};
    
    std::unordered_map<std::string, std::filesystem::path> m_ProtocolMap;
    std::vector<std::filesystem::path> m_SearchPaths;
};
}
