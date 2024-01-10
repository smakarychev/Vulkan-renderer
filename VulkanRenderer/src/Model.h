#pragma once
#include <vector>

#include "HandleArray.h"
#include "Mesh.h"
#include "ModelAsset.h"
#include "RenderObject.h"

class Scene;
class ShaderPipelineTemplate;
class Renderer;

class Model
{
    struct MaterialInfo
    {
        using MaterialType = assetLib::ModelInfo::MaterialType;
        MaterialType Type;
        glm::vec4 AlbedoColor;
        std::vector<std::string> AlbedoTextures;
        std::vector<std::string> NormalTextures;
    };
    struct MeshInfo
    {
        Mesh Mesh;
        MaterialInfo Material;
    };
public:
    static Model* LoadFromAsset(std::string_view path);
    void CreateRenderObjects(Scene* scene, const glm::mat4& transform);

    void CreateDebugBoundingSpheres(Scene* scene, const glm::mat4& transform);
private:
    std::vector<MeshInfo> m_Meshes;
    std::string m_ModelName;

    bool m_IsInScene{false};
    std::vector<RenderObject> m_RenderObjects;
};

class ModelCollection
{
    void AddModel(Model* model);
    
private:
    struct ModelInfo
    {
        Model* Model;
        std::vector<RenderObject> RenderObjects;
    };
    std::vector<ModelInfo> m_Models;
    HandleArray<Mesh> m_Meshes;
    HandleArray<Texture> m_Textures;
    HandleArray<MaterialGPU> m_MaterialsGPU;
    std::vector<RenderHandle<Texture>> m_BindlessTextures;
};