#pragma once

#include "AssetLib.h"

#include "core.h"

#include <tiny_gltf.h>
#include <filesystem>

namespace assetLib
{
    struct SceneInfo
    {
        std::filesystem::path Path;
        tinygltf::Model Scene{};
    };

    std::optional<SceneInfo> readSceneHeader(const std::filesystem::path& path);
    bool readSceneBinary(SceneInfo& sceneInfo);
}