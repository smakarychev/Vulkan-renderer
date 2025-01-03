﻿#pragma once
#include <string>
#include <unordered_map>

class Image;
class Model;
class ShaderReflection;

class AssetManager
{
public:
    static std::string GetShaderKey(const std::vector<std::string_view>& paths);
    static ShaderReflection* GetShader(const std::string& name);
    static void AddShader(const std::string& name, const ShaderReflection& shader);
    static void RemoveShader(const std::string& name);

    static Model* GetModel(const std::string& name);
    static void AddModel(const std::string& name, const Model& model);

    static Image* GetImage(const std::string& name);
    static void AddImage(const std::string& name, const Image& image);
    
private:
    static std::unordered_map<std::string, ShaderReflection> s_Shaders;
    static std::unordered_map<std::string, Model> s_Models;
    static std::unordered_map<std::string, Image> s_Images;
};
