#pragma once

#include "Bakers/BakerContext.h"
#include "Bakers/Bakers.h"
#include "v2/Scenes/SceneAsset.h"

namespace lux::bakers
{
struct SceneBakeSettings
{
};

class SceneBaker
{
public:
    static std::filesystem::path GetBakedPath(const std::filesystem::path& originalFile,
        const SceneBakeSettings& settings, const Context& ctx);

    IoResult<std::filesystem::path> BakeToFile(const std::filesystem::path& path,
        const SceneBakeSettings& settings, const Context& ctx);

    IoResult<assetlib::SceneAsset> Bake(const std::filesystem::path& path,
        const SceneBakeSettings& settings, const Context& ctx);

    bool ShouldBake(const std::filesystem::path& path, const SceneBakeSettings& settings, const Context& ctx);
private:
    static constexpr std::string_view POST_BAKE_EXTENSION = SCENE_ASSET_EXTENSION;
};
}