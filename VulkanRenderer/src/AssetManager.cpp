#include "AssetManager.h"

#include "Rendering/Shader/ShaderPipelineTemplate.h"
#include "Scene/Scene.h"

StringUnorderedMap<ShaderReflection> AssetManager::s_Shaders = {};
StringUnorderedMap<Image> AssetManager::s_Images = {};
StringUnorderedMap<SceneInfo> AssetManager::s_Scenes = {};

void AssetManager::Shutdown()
{
    s_Shaders.clear();
    s_Images.clear();
}

std::string AssetManager::GetShaderKey(const std::vector<std::string>& paths)
{
    std::string key;
    for (auto& path : paths)
        key += std::string{path};

    return key;
}

ShaderReflection* AssetManager::GetShader(std::string_view name)
{
    auto it = s_Shaders.find(name);

    return it == s_Shaders.end() ? nullptr : &it->second;
}

ShaderReflection* AssetManager::AddShader(std::string_view name, ShaderReflection&& shader)
{
    auto it = s_Shaders.find(name);
    if (it == s_Shaders.end())
        return &s_Shaders.emplace(name, std::move(shader)).first->second;

    it->second = std::move(shader);
    
    return &it->second;
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
