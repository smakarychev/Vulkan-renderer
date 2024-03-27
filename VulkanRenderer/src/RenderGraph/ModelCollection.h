#pragma once

#include "HandleArray.h"
#include "RenderHandle.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

class ShaderDescriptors;
class Image;
class Mesh;
struct Material;
struct MaterialGPU;
struct RenderObject;
class ShaderDescriptorSet;
class Model;

class ModelCollection
{
public:
    struct ModelInstanceInfo
    {
        glm::mat4 Transform;
    };
    
public:
    void CreateDefaultTextures();
    void RegisterModel(Model* model, const std::string& name);
    void AddModelInstance(const std::string& modelName, const ModelInstanceInfo& modelInstanceInfo);
    void ApplyMaterialTextures(ShaderDescriptorSet& bindlessDescriptorSet) const;
    void ApplyMaterialTextures(ShaderDescriptors& bindlessDescriptors) const;
        
    template <typename Filter, typename Callback>
    void FilterRenderObjects(Filter&& filterFn, Callback&& callbackFn) const;

    const std::vector<RenderObject>& GetRenderObjects(const std::string& modelName) const;

    const HandleArray<Mesh>& GetMeshes() const { return m_Meshes; }
    const HandleArray<MaterialGPU>& GetMaterialsGPU() const { return m_MaterialsGPU; }
    const HandleArray<Material>& GetMaterials() const { return m_Materials; }
    
private:
    std::vector<RenderObject> CreateRenderObjects(const Model* model);
    RenderHandle<MaterialGPU> AddMaterialGPU(const MaterialGPU& material);
    RenderHandle<Material> AddMaterial(const Material& material);
    RenderHandle<Mesh> AddMesh(const Mesh& mesh);
    RenderHandle<Image> AddTexture(const Image& texture);

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
    HandleArray<Image> m_Textures;
    HandleArray<MaterialGPU> m_MaterialsGPU;
    HandleArray<Material> m_Materials;

    RenderHandle<Image> m_WhiteTexture{}; 
    RenderHandle<Image> m_NormalMapTexture{}; 
    
    std::vector<RenderObject> m_RenderObjects;
};

template <typename Filter, typename Callback>
void ModelCollection::FilterRenderObjects(Filter&& filterFn, Callback&& callbackFn) const
{
    for (auto& renderObject : m_RenderObjects)
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

