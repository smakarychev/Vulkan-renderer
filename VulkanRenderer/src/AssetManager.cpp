#include "AssetManager.h"

#include "Model.h"
#include "Rendering/Shader.h"

std::unordered_map<std::string, Shader> AssetManager::s_Shaders = {};
std::unordered_map<std::string, Model> AssetManager::s_Models = {};
std::unordered_map<std::string, Image> AssetManager::s_Images = {};

std::string AssetManager::GetShaderKey(const std::vector<std::string_view>& paths)
{
    std::string key;
    for (auto& path : paths)
        key += std::string{path};

    return key;
}

Shader* AssetManager::GetShader(const std::string& name)
{
    auto it = s_Shaders.find(name);

    return it == s_Shaders.end() ? nullptr : &it->second;
}

void AssetManager::AddShader(const std::string& name, const Shader& shader)
{
    s_Shaders.emplace(name, shader);
}

void AssetManager::RemoveShader(const std::string& name)
{
    s_Shaders.erase(name);
}

Model* AssetManager::GetModel(const std::string& name)
{
    auto it = s_Models.find(name);

    return it == s_Models.end() ? nullptr : &it->second;
}

void AssetManager::AddModel(const std::string& name, const Model& model)
{
    s_Models.emplace(name, model);
}

Image* AssetManager::GetImage(const std::string& name)
{
    auto it = s_Images.find(name);

    return it == s_Images.end() ? nullptr : &it->second;
}

void AssetManager::AddImage(const std::string& name, const Image& image)
{
    s_Images.emplace(name, image);
}
