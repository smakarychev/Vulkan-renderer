#include "SceneAsset.h"

#include "core.h"

#include <glm/gtx/quaternion.hpp>

namespace assetLib
{
    std::optional<SceneInfo> readSceneHeader(const std::filesystem::path& path)
    {
        tinygltf::Model gltf;
        tinygltf::TinyGLTF loader;
        loader.SetStoreOriginalJSONForExtrasAndExtensions(true);
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

    glm::mat4 getTransform(tinygltf::Node& node)
    {
        if (!node.matrix.empty())
            return *(glm::dmat4*)node.matrix.data();

        glm::vec3 translation{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};

        if (!node.translation.empty())
            translation = *(glm::dvec3*)node.translation.data();
        if (!node.rotation.empty())
        {
            rotation.x = (f32)node.rotation[0];
            rotation.y = (f32)node.rotation[1];
            rotation.z = (f32)node.rotation[2];
            rotation.w = (f32)node.rotation[3];
        }
        if (!node.scale.empty())
            scale = *(glm::dvec3*)node.scale.data();

        return
            glm::translate(glm::mat4(1.0f), translation) *
            glm::toMat4(rotation) *
            glm::scale(glm::mat4(1.0f), scale);
    }
}
