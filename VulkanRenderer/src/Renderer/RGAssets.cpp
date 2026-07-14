#include "rendererpch.h"

#include "RGAssets.h"

#include "Assets/Images/ImageAssetManager.h"
#include "RenderGraph/RGGraph.h"

namespace lux
{
void PersistentImageAsset::Load(RG::Graph& graph, ImageAssetManager& manager, const std::filesystem::path& path)
{
    AssetHandle = manager.LoadResource({path});
    Image = graph.AddPersistent(manager.Get(AssetHandle), ImageLayout::Readonly);
}

PersistentImageAsset& PersistentImageAsset::Update(RG::Graph& graph, ImageAssetManager& manager)
{
    if (Layout.has_value())
        graph.UpdatePersistent(Image, manager.Get(AssetHandle), *Layout);
    else
        graph.UpdatePersistent(Image, manager.Get(AssetHandle));
    Layout = std::nullopt;
    
    return *this;
}

PersistentImageAssetHandle PersistentImageAssetMap::Add(PersistentImageAsset& persistentImageAsset)
{
    const ImageHandle handle = persistentImageAsset.AssetHandle;
    m_Map[handle] = &persistentImageAsset;
    
    return handle;
}

void PersistentImageAssetMap::SetLayout(PersistentImageAssetHandle handle, ImageLayout layout) const
{
    auto it = m_Map.find(handle);
    if (it == m_Map.end())
        return;
    
    it->second->Layout = layout;
}
}
