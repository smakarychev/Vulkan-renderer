#pragma once
#include <string>
#include <unordered_map>

#include "Rendering/Image/Image.h"
#include "String/StringHeterogeneousHasher.h"

class SceneInfo;
class ShaderReflection;

class AssetManager
{
public:
    static void Shutdown();
    
    static std::string GetShaderKey(const std::vector<std::string>& paths);
    static ShaderReflection* GetShader(std::string_view name);
    static ShaderReflection* AddShader(std::string_view name, ShaderReflection&& shader);
    static void RemoveShader(std::string_view name);

    static Image GetImage(std::string_view name);
    static void AddImage(std::string_view, Image image);

    static SceneInfo* GetSceneInfo(std::string_view name);
    static SceneInfo* AddSceneInfo(std::string_view name, SceneInfo&& sceneInfo);
    
private:
    static StringUnorderedMap<ShaderReflection> s_Shaders;
    static StringUnorderedMap<Image> s_Images;
    
    static StringUnorderedMap<SceneInfo> s_Scenes;
};
