#pragma once

#include "ImageAsset.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPath.h"
#include "Assets/Common/AssetFreeList.h"

#include <AssetImportLib/Importers/ImportContext.h>

struct FrameContext;

namespace lux
{
namespace import
{
class ImageImporter;
}

template <>
struct ResourceAssetLoadParameters<ImageAsset>
{
    AssetPath Path{};
    auto operator<=>(const ResourceAssetLoadParameters&) const = default;
};
using ImageLoadParameters = ResourceAssetLoadParameters<ImageAsset>;

template <>
struct ResourceAssetLoadParametersHasher<ImageAsset>
{
    usize operator()(const ImageLoadParameters& parameters) const noexcept
    {
        return std::hash<AssetPath>{}(parameters.Path);
    }
};

class ImageAssetManager final : public ResourceAssetManager<ImageAsset, ResourceAssetTraitsGetValue>
{
public:
    LUX_ASSET_MANAGER(ImageAssetManager, "7fbed3b1-ca4c-4d90-a678-ec202cf04ea3"_guid)

    bool AddManaged(const assetlib::AssetMetadata& metadata, const std::filesystem::path& metaPath) override;
    bool Imports(std::string_view extension) override;
    void OnFileModified(const AssetPath& path) override;

    void Shutdown();

    void OnFrameBegin(FrameContext& ctx);

protected:
    ImageHandle LoadAsset(const ImageLoadParameters& parameters) override;
    void ReloadAsset(ImageHandle handle) override;
    void UnloadAsset(ImageHandle handle) override;
    GetType GetAsset(ImageHandle handle) const override;

private:
    ImageAsset DoLoad(import::ImageImporter& importer, const AssetPath& path) const;

private:
    AssetFreeList<ImageAsset> m_Images;

    /* for hot-reloading */
    DeletionQueue* m_FrameDeletionQueue{nullptr};
};
}
