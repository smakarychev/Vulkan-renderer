#pragma once

#include "Assets/Images/ImageAsset.h"
#include "RenderGraph/RGResource.h"

namespace lux
{
class ImageAssetManager;

struct PersistentImageAsset
{
    ImageHandle AssetHandle{};
    RG::PersistentImageResource Image{};
    std::optional<ImageLayout> Layout{};
    
    void Load(RG::Graph& graph, ImageAssetManager& manager, const std::filesystem::path& path);
    PersistentImageAsset& Update(RG::Graph& graph, ImageAssetManager& manager);
    constexpr bool HasValue() const { return Image.HasValue(); }
};

using PersistentImageAssetHandle = ImageHandle;

class PersistentImageAssetMap
{
public:
    PersistentImageAssetHandle Add(PersistentImageAsset& persistentImageAsset);
    void SetLayout(PersistentImageAssetHandle handle, ImageLayout layout) const;
private:
    std::unordered_map<ImageHandle, PersistentImageAsset*> m_Map; 
};
}
