#include "SceneAsset.h"

#include "core.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <nlohmann/json.hpp>

namespace glm
{
    void to_json(nlohmann::json& j, const vec4& vec)
    {
        j = { { "r", vec.x }, { "g", vec.y }, { "b", vec.z }, { "a", vec.w } };
    }
    
    void from_json(const nlohmann::json& j, vec4& vec)
    {
        vec.x = j.at("r").get<f32>();
        vec.y = j.at("g").get<f32>();
        vec.z = j.at("b").get<f32>();
        vec.w = j.at("a").get<f32>();
    }

    void to_json(nlohmann::json& j, const vec3& vec)
    {
        j = { { "x", vec.x }, { "y", vec.y }, { "z", vec.z } };
    }
    
    void from_json(const nlohmann::json& j, vec3& vec)
    {
        vec.x = j.at("x").get<f32>();
        vec.y = j.at("y").get<f32>();
        vec.z = j.at("z").get<f32>();
    }
}

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
            LUX_LOG_ERROR("{} ({})", errors, path.string());
        if (!warnings.empty())
            LUX_LOG_WARN("{} ({})", warnings, path.string());
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
            LUX_LOG_ERROR("{} ({})", errors, path.string());
        if (!warnings.empty())
            LUX_LOG_WARN("{} ({})", warnings, path.string());

        return success;
    }

    Transform3d getTransform(tinygltf::Node& node)
    {
        glm::dvec3 translation{0.0f};
        glm::dquat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::dvec3 scale{1.0f};
        
        if (!node.matrix.empty())
        {
            glm::dvec3 scew;
            glm::dvec4 perspective;
            glm::decompose(*(glm::dmat4*)node.matrix.data(), scale, rotation, translation, scew, perspective);
        }
        else
        {
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
        }

        return Transform3d{
            .Position = translation,
            .Orientation = rotation,
            .Scale = scale};
    }
}
