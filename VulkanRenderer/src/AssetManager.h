#pragma once
#include <string>
#include <unordered_map>

#include "Rendering/Image/Image.h"
#include "utils/HashedString.h"

class Model;
class ShaderReflection;

class AssetManager
{
public:
    static void Shutdown();
    
    static std::string GetShaderKey(const std::vector<std::string>& paths);
    static ShaderReflection* GetShader(std::string_view name);
    static void AddShader(std::string_view name, ShaderReflection&& shader);
    static void RemoveShader(std::string_view name);

    static Model* GetModel(std::string_view);
    static void AddModel(std::string_view, const Model& model);

    static Image GetImage(std::string_view name);
    static void AddImage(std::string_view, Image image);
    
private:
    static Utils::StringUnorderedMap<ShaderReflection> s_Shaders;
    static Utils::StringUnorderedMap<Model> s_Models;
    static Utils::StringUnorderedMap<Image> s_Images;
};
