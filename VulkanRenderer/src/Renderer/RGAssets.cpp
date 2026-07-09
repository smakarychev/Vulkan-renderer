#include "rendererpch.h"

#include "RGAssets.h"

#include "Assets/Images/ImageAssetManager.h"
#include "RenderGraph/RGGraph.h"

namespace lux
{
void ExternalImageAsset::Load(RG::Graph& graph, ImageAssetManager& manager, const std::filesystem::path& path)
{
    AssetHandle = manager.LoadResource({path});
    Image = graph.AddPersistent(manager.Get(AssetHandle), ImageLayout::Readonly);
}

ExternalImageAsset& ExternalImageAsset::Update(RG::Graph& graph, ImageAssetManager& manager)
{
    // todo: I believe we need to reset layout to ReadOnly if asset was hot-reloaded
    graph.UpdatePersistent(Image, manager.Get(AssetHandle));
    
    return *this;
}
}
