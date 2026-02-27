#pragma once
#include "MaterialAsset.h"
#include "Assets/AssetManager.h"
#include "Assets/Common/AssetFreeListMap.h"
#include "Containers/FreeList.h"

#include <glm/glm.hpp>

namespace lux
{
class ImageAssetManager;

template <>
struct ResourceAssetLoadParameters<MaterialAsset>
{
    std::filesystem::path Path{};
};

using MaterialLoadParameters = ResourceAssetLoadParameters<MaterialAsset>;

class MaterialAssetManager final : public ResourceAssetManager<MaterialAsset, ResourceAssetTraits>
{
public:
    LUX_ASSET_MANAGER(MaterialAssetManager, "627a4f5c-3219-4a25-89f7-763e0517d0af"_guid)

    bool AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver) override;
    bool Bakes(const std::filesystem::path& path) override;
    void OnFileModified(const std::filesystem::path& path) override;

protected:
    MaterialHandle LoadAsset(const MaterialLoadParameters& parameters) override;
    void UnloadAsset(MaterialHandle handle) override;
    const MaterialAsset* GetAsset(MaterialHandle handle) const override;
    
private:
    std::optional<MaterialAsset> DoLoad(const MaterialLoadParameters& parameters) const;
    ImageHandle LoadTexture(ImageAssetManager* imageAssetManager, assetlib::AssetId imageAsset) const;

private:
    AssetFreeListMap<MaterialAsset> m_Materials;
};
}
