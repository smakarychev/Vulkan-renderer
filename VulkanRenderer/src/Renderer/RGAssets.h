#pragma once

#include "Assets/Images/ImageAsset.h"
#include "RenderGraph/RGResource.h"

namespace lux
{
class ImageAssetManager;

struct ExternalImageAsset
{
    ImageHandle AssetHandle{};
    RG::PersistentImageResource Image{};
    
    void Load(RG::Graph& graph, ImageAssetManager& manager, const std::filesystem::path& path);
    ExternalImageAsset& Update(RG::Graph& graph, ImageAssetManager& manager);
    constexpr bool HasValue() const { return Image.HasValue(); }
};
}
