﻿#include "AssetManager.h"

#include "Model.h"
#include "Vulkan/Shader.h"

std::unordered_map<std::string, Shader> AssetManager::s_Shaders = {};
std::unordered_map<std::string, Model> AssetManager::s_Models = {};

Shader* AssetManager::GetShader(const std::string& name)
{
    auto it = s_Shaders.find(name);
    return it == s_Shaders.end() ? nullptr : &it->second;
}

void AssetManager::AddShader(const std::string& name, const Shader& shader)
{
    s_Shaders.emplace(name, shader);
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