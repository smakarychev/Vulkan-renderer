#pragma once

#include "ImageAsset.h"
#include "Assets/AssetManager.h"
#include "Assets/Common/AssetFreeListMap.h"

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
    std::filesystem::path Path{};
};

using ImageLoadParameters = ResourceAssetLoadParameters<ImageAsset>;

class ImageAssetManager final : public ResourceAssetManager<ImageAsset, ResourceAssetTraitsGetValue>
{
public:
    LUX_ASSET_MANAGER(ImageAssetManager, "7fbed3b1-ca4c-4d90-a678-ec202cf04ea3"_guid)

    bool AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver) override;
    bool Imports(std::string_view extension) override;
    void OnFileModified(const std::filesystem::path& path) override;

    void Shutdown();

    void OnFrameBegin(FrameContext& ctx);

protected:
    ImageHandle LoadAsset(const ImageLoadParameters& parameters) override;
    void UnloadAsset(ImageHandle handle) override;
    GetType GetAsset(ImageHandle handle) const override;

private:
    void OnRawFileModified(const std::filesystem::path& path);
    ImageAsset DoLoad(import::ImageImporter& importer, const std::filesystem::path& path) const;

private:
    // todo: alternative is to make ImageHandle equivalent to Image (since both are handles)
    // todo: this will require support from Device to swap things (already exists in method `ResizeBuffer`)
    AssetFreeListMap<ImageAsset> m_Images;

    /* for hot-reloading */
    DeletionQueue* m_FrameDeletionQueue{nullptr};
};
}
