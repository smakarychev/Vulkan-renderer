#include "rendererpch.h"

#include "AssetManager.h"

#include "Rendering/Shader/ShaderPipelineTemplate.h"
#include "Scene/Scene.h"

StringUnorderedMap<Image> AssetManager::s_Images = {};
StringUnorderedMap<SceneInfo> AssetManager::s_Scenes = {};

void AssetManager::Shutdown()
{
    s_Images.clear();
}

Image AssetManager::GetImage(std::string_view name)
{
    auto it = s_Images.find(name);

    return it == s_Images.end() ? Image{} : it->second;
}

void AssetManager::AddImage(std::string_view name, Image image)
{
    s_Images.emplace(name, image);
}

SceneInfo* AssetManager::GetSceneInfo(std::string_view name)
{
    auto it = s_Scenes.find(name);

    return it == s_Scenes.end() ? nullptr : &it->second;
}

SceneInfo* AssetManager::AddSceneInfo(std::string_view name, SceneInfo&& sceneInfo)
{
    auto&& [scene, _] = s_Scenes.emplace(name, std::move(sceneInfo));

    return &scene->second;
}
