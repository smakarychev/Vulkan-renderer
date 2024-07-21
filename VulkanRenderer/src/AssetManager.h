#pragma once
#include <string>
#include <unordered_map>

class Image;
class Model;
class Shader;

class AssetManager
{
public:
    static std::string GetShaderKey(const std::vector<std::string_view>& paths);
    static Shader* GetShader(const std::string& name);
    static void AddShader(const std::string& name, const Shader& shader);
    static void RemoveShader(const std::string& name);

    static Model* GetModel(const std::string& name);
    static void AddModel(const std::string& name, const Model& model);

    static Image* GetImage(const std::string& name);
    static void AddImage(const std::string& name, const Image& image);
    
private:
    static std::unordered_map<std::string, Shader> s_Shaders;
    static std::unordered_map<std::string, Model> s_Models;
    static std::unordered_map<std::string, Image> s_Images;
};
