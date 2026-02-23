#pragma once
#include "Assets/AssetManager.h"
#include "Bakers/BakerContext.h"
#include "Containers/FreeList.h"
#include "Rendering/Image/Image.h"
#include "String/StringHeterogeneousHasher.h"

struct FrameContext;

namespace lux
{
namespace bakers
{
struct ImageBakeSettings;
}

template <>
struct ResourceAssetLoadParameters<Image>
{
    std::filesystem::path Path{};
};

using ImageLoadParameters = ResourceAssetLoadParameters<Image>;

using ImageHandle = AssetHandle<Image>;

class ImageAssetManager final : public ResourceAssetManager<Image, ResourceAssetTraitsGetValue>
{
public:
    LUX_ASSET_MANAGER(ImageAssetManager, "7fbed3b1-ca4c-4d90-a678-ec202cf04ea3"_guid)

    bool AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver) override;
    bool Bakes(const std::filesystem::path& path) override;
    void OnFileModified(const std::filesystem::path& path) override;

    void Init(const bakers::ImageBakeSettings& bakeSettings);
    void Shutdown();

    void OnFrameBegin(FrameContext& ctx);

protected:
    ImageHandle LoadAsset(const ImageLoadParameters& parameters) override;
    void UnloadAsset(ImageHandle handle) override;
    GetType GetAsset(ImageHandle handle) const override;

private:
    void OnRawFileModified(const std::filesystem::path& path);
    void OnBakedFileModified(const std::filesystem::path& path);
    Image DoLoad(const ImageLoadParameters& parameters) const;

private:
    // todo: alternative is to make ImageHandle equivalent to Image (since both are handles)
    // todo: this will require support from Device to swap things (already exists in method `ResizeBuffer`)
    FreeList<Image> m_Images;
    std::unordered_map<std::filesystem::path, ImageHandle> m_ImagesMap;
    std::vector<std::filesystem::path> m_HandlesToPaths;

    /* for hot-reloading */
    bakers::Context m_Context{};
    const bakers::ImageBakeSettings* m_BakeSettings{nullptr};
    DeletionQueue* m_FrameDeletionQueue{nullptr};
};
}
