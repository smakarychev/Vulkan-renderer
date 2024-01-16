#pragma once
#include <ranges>
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
    friend class ModelCollection;
    struct MaterialInfo
    {
        using MaterialType = assetLib::ModelInfo::MaterialType;
        using MaterialPropertiesPBR = assetLib::ModelInfo::MaterialPropertiesPBR;
        MaterialType Type;
        MaterialPropertiesPBR PropertiesPBR;
        std::vector<std::string> AlbedoTextures;
        std::vector<std::string> NormalTextures;
        std::vector<std::string> MetallicRoughnessTextures;
        std::vector<std::string> AmbientOcclusionTextures;
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

    // TODO: remove me once the ModelCollection class is finished
    bool m_IsInScene{false};
    std::vector<RenderObject> m_RenderObjects;
};

class ModelCollection
{
public:
    void AddModel(Model* model, const std::string& name);
    void ApplyMaterialTextures(ShaderDescriptorSet& bindlessDescriptorSet) const;

    template <typename Filter, typename Callback>
    void FilterRenderObjects(const std::string& modelName, Filter&& filterFn, Callback&& callbackFn) const;
    
private:
    std::vector<RenderObject> CreateRenderObjects(Model* model);
    RenderHandle<MaterialGPU> AddMaterialGPU(const MaterialGPU& material);
    RenderHandle<Mesh> AddMesh(const Mesh& mesh);
    RenderHandle<Texture> AddTexture(const Texture& texture);

    template <typename T>
    RenderHandle<T> AddRenderHandle(const T& object, HandleArray<T>& array);
    
private:
    struct ModelInfo
    {
        Model* Model;
        std::vector<RenderObject> RenderObjects;
    };
    std::unordered_map<std::string, ModelInfo> m_Models;
    HandleArray<Mesh> m_Meshes;
    HandleArray<Texture> m_Textures;
    HandleArray<MaterialGPU> m_MaterialsGPU;
};

template <typename Filter, typename Callback>
void ModelCollection::FilterRenderObjects(const std::string& modelName, Filter&& filterFn, Callback&& callbackFn) const
{
    auto& model = m_Models.at(modelName);
    for (auto& renderObject : model.RenderObjects)
        if (filterFn(renderObject))
            callbackFn(renderObject);
}

template <typename T>
RenderHandle<T> ModelCollection::AddRenderHandle(const T& object, HandleArray<T>& array)
{
    RenderHandle<T> handle = (u32)array.size();
    array.push_back(object);
    return handle;
}
