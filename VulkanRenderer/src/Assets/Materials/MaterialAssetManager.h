#pragma once
#include "MaterialAsset.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPath.h"
#include "Assets/Common/AssetFreeList.h"

namespace lux
{
namespace import
{
class MaterialImporter;
struct Context;
}

class ImageAssetManager;

template <>
struct ResourceAssetLoadParameters<MaterialAsset>
{
    AssetPath Path{};
    auto operator<=>(const ResourceAssetLoadParameters&) const = default;
};
using MaterialLoadParameters = ResourceAssetLoadParameters<MaterialAsset>;

template <>
struct ResourceAssetLoadParametersHasher<MaterialAsset>
{
    usize operator()(const MaterialLoadParameters& parameters) const noexcept
    {
        return std::hash<AssetPath>{}(parameters.Path);
    }
};

class MaterialAssetManager final : public ResourceAssetManager<MaterialAsset, ResourceAssetTraits>
{
public:
    LUX_ASSET_MANAGER(MaterialAssetManager, "627a4f5c-3219-4a25-89f7-763e0517d0af"_guid)

    void OnAssetSystemInit() override;
    bool AddManaged(const assetlib::AssetMetadata& metadata, const std::filesystem::path& metaPath) override;
    bool Imports(std::string_view extension) override;
    void OnFileModified(const AssetPath& path) override;

protected:
    MaterialHandle LoadAsset(const MaterialLoadParameters& parameters) override;
    void ReloadAsset(MaterialHandle handle) override;
    void UnloadAsset(MaterialHandle handle) override;
    const MaterialAsset* GetAsset(MaterialHandle handle) const override;
    
private:
    std::optional<MaterialAsset> DoLoad(import::MaterialImporter& importer, const AssetPath& path) const;
    ImageHandle LoadTexture(assetlib::AssetId imageAsset) const;

private:
    AssetFreeList<MaterialAsset> m_Materials;
    ImageAssetManager* m_ImageAssetManager{nullptr};
};
}
