#include "AssetManager.h"

#include "Model.h"
#include "Rendering/Shader/Shader.h"

Utils::StringUnorderedMap<ShaderReflection> AssetManager::s_Shaders = {};
Utils::StringUnorderedMap<Model> AssetManager::s_Models = {};
Utils::StringUnorderedMap<Image> AssetManager::s_Images = {};

void AssetManager::Shutdown()
{
    s_Shaders.clear();
    s_Models.clear();
    s_Images.clear();
}

std::string AssetManager::GetShaderKey(const std::vector<std::string_view>& paths)
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

void AssetManager::AddShader(std::string_view name, ShaderReflection&& shader)
{
    s_Shaders.emplace(name, std::move(shader));
}

void AssetManager::RemoveShader(std::string_view name)
{
    s_Shaders.erase(name);
}

Model* AssetManager::GetModel(std::string_view name)
{
    auto it = s_Models.find(name);

    return it == s_Models.end() ? nullptr : &it->second;
}

void AssetManager::AddModel(std::string_view name, const Model& model)
{
    s_Models.emplace(name, model);
}

Image* AssetManager::GetImage(std::string_view name)
{
    auto it = s_Images.find(name);

    return it == s_Images.end() ? nullptr : &it->second;
}

void AssetManager::AddImage(std::string_view name, const Image& image)
{
    s_Images.emplace(name, image);
}
