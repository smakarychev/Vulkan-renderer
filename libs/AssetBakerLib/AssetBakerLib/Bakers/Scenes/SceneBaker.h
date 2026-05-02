#pragma once

#include <AssetLib/Scenes/SceneAsset.h>
#include <AssetBakerLib/Bakers/BakerContext.h>
#include <AssetBakerLib/Bakers/Bakers.h>

namespace lux::assetlib
{
struct SceneMeta;
}

namespace lux::bakers
{
class SceneBaker
{
public:
    SceneBaker(const std::shared_ptr<Context>& ctx) : m_Ctx(ctx) {}
    
    std::filesystem::path GetBakedPath(const std::filesystem::path& metaPath) const;
    std::filesystem::path GetBakedPath(const assetlib::SceneMeta& meta) const;
    
    IoResult<std::filesystem::path> BakeToFile(assetlib::SceneMeta& meta, const std::filesystem::path& metaPath);

    bool ShouldBake(const std::filesystem::path& metaPath) const;
private:
    IoResult<assetlib::SceneAsset> Bake(const assetlib::SceneMeta& meta);
private:
    static constexpr std::string_view POST_BAKE_EXTENSION = SCENE_ASSET_EXTENSION;
    std::shared_ptr<Context> m_Ctx{nullptr};
};
}
