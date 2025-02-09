#include "SceneAsset.h"

namespace assetLib
{
    std::optional<SceneInfo> readSceneHeader(const std::filesystem::path& path)
    {
        tinygltf::Model gltf;
        tinygltf::TinyGLTF loader;
        loader.ShouldPreloadBuffersData(false);
        loader.ShouldPreloadImagesData(false);
        std::string errors;
        std::string warnings;
        bool success = loader.LoadASCIIFromFile(&gltf, &errors, &warnings, path.string());
        if (!errors.empty())
            LOG("ERROR: {} ({})", errors, path.string());
        if (!warnings.empty())
            LOG("WARNING: {} ({})", warnings, path.string());
        if (!success)
            return std::nullopt;

        return SceneInfo{
            .Path = path,
            .Scene = gltf};
    }

    bool readSceneBinary(SceneInfo& sceneInfo)
    {
        auto&& [path, gltf] = sceneInfo;
        tinygltf::TinyGLTF loader;

        std::string errors;
        std::string warnings;
        bool success = loader.LoadBuffersData(&gltf, &errors, &warnings, path.parent_path().string());
        success = success && loader.LoadImagesData(&gltf, &errors, &warnings, path.parent_path().string());
        if (!errors.empty())
            LOG("ERROR: {} ({})", errors, path.string());
        if (!warnings.empty())
            LOG("WARNING: {} ({})", warnings, path.string());

        return success;
    }
}
