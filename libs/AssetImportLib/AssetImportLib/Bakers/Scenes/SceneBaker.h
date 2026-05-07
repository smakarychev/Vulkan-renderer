#pragma once

#include <AssetLib/Scenes/Scene/SceneMeta.h>
#include <AssetLib/Scenes/Scene/SceneAsset.h>
#include <AssetImportLib/Importers/ImportContext.h>

namespace lux::assetlib
{
struct SceneMeta;
}

namespace lux::import
{
class SceneBaker
{
public:
    SceneBaker(const std::shared_ptr<Context>& ctx) : m_Ctx(ctx) {}
    
    IoResult<std::filesystem::path> BakeToFile(assetlib::SceneMeta& meta, const std::filesystem::path& metaPath);

    bool NeedsBaking(const std::filesystem::path& metaPath) const;
private:
    IoResult<assetlib::SceneAsset> Bake(const assetlib::SceneMeta& meta);
private:
    std::shared_ptr<Context> m_Ctx{nullptr};
};
}
