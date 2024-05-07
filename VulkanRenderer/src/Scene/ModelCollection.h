#pragma once

#include "RenderHandleArray.h"
#include "RenderHandle.h"
#include "RenderObject.h"

#include <string>
#include <unordered_map>
#include <vector>

class ShaderDescriptors;
class Image;
class Mesh;
class ShaderDescriptorSet;
class Model;

class ModelCollection
{
public:
    struct ModelInstanceInfo
    {
        RenderObjectTransform Transform;
    };
    /* indices into render objects array of model collection;
     * used for sorting, and any other dynamic operation,
     * where cpu-side of data (transforms, materials, etc.) is needed
     */
    using RenderObjectIndices = std::vector<u32>;
    using RenderObjectPermutation = std::vector<u32>;
public:
    void CreateDefaultTextures();
    void RegisterModel(Model* model, const std::string& name);
    void AddModelInstance(const std::string& modelName, const ModelInstanceInfo& modelInstanceInfo);
    void ApplyMaterialTextures(ShaderDescriptorSet& bindlessDescriptorSet) const;
    void ApplyMaterialTextures(ShaderDescriptors& bindlessDescriptors) const;

    /* Filter is (const Mesh&, const Material&) -> bool; Callback is (const RenderObject&, u32 index) */ 
    template <typename Filter, typename Callback>
    void FilterRenderObjects(Filter&& filterFn, Callback&& callbackFn) const;

    /* Filter is (const Mesh&, const Material&) -> bool; Callback is (const Mesh&, u32 index). 
     * The difference between render objects and meshes is that meshes are unique
     * each instantiation of model creates new render objects, but not new meshes
     */ 
    template <typename Filter, typename Callback>
    void FilterMeshes(Filter&& filterFn, Callback&& callbackFn) const;

    template <typename Callback>
    void IterateRenderObjects(const RenderObjectIndices& indices, Callback&& callback) const;
    template <typename Callback>
    void IterateRenderObjects(const RenderObjectIndices& indices, const RenderObjectPermutation& permutation,
        Callback&& callback) const;

    const std::vector<RenderObject>& GetRenderObjects(const std::string& modelName) const;

    const RenderHandleArray<Mesh>& GetMeshes() const { return m_Meshes; }
    const RenderHandleArray<MaterialGPU>& GetMaterialsGPU() const { return m_MaterialsGPU; }
    const RenderHandleArray<Material>& GetMaterials() const { return m_Materials; }
private:
    std::vector<RenderObject> CreateRenderObjects(const Model* model);
    RenderHandle<MaterialGPU> AddMaterialGPU(const MaterialGPU& material);
    RenderHandle<Material> AddMaterial(const Material& material);
    RenderHandle<Mesh> AddMesh(const Mesh& mesh);
    RenderHandle<Image> AddTexture(const Image& texture);

    template <typename T>
    RenderHandle<T> AddRenderHandle(const T& object, RenderHandleArray<T>& array);
    
private:
    struct ModelInfo
    {
        Model* Model;
        std::vector<RenderObject> RenderObjects;
    };
    std::unordered_map<std::string, ModelInfo> m_Models;
    RenderHandleArray<Mesh> m_Meshes;
    RenderHandleArray<Image> m_Textures;
    /* is used to not push identical textures into `m_Textures` array */
    std::unordered_map<std::string, RenderHandle<Image>> m_TexturesMap;
    RenderHandleArray<MaterialGPU> m_MaterialsGPU;
    RenderHandleArray<Material> m_Materials;

    RenderHandle<Image> m_WhiteTexture{}; 
    RenderHandle<Image> m_BlackTexture{}; 
    RenderHandle<Image> m_NormalMapTexture{}; 
    
    std::vector<RenderObject> m_RenderObjects;
};

template <typename Filter, typename Callback>
void ModelCollection::FilterRenderObjects(Filter&& filterFn, Callback&& callbackFn) const
{
    for (u32 i = 0; i < m_RenderObjects.size(); i++)
        if (filterFn(GetMeshes()[m_RenderObjects[i].Mesh], GetMaterials()[m_RenderObjects[i].Material]))
            callbackFn(m_RenderObjects[i], i);
}

template <typename Filter, typename Callback>
void ModelCollection::FilterMeshes(Filter&& filterFn, Callback&& callbackFn) const
{
    for (u32 i = 0; i < m_Meshes.size(); i++)
        if (filterFn(m_Meshes[i], m_Materials[i]))
            callbackFn(m_Meshes[i], i);
}

template <typename Callback>
void ModelCollection::IterateRenderObjects(const RenderObjectIndices& indices, Callback&& callback) const
{
    for (u32 i = 0; i < indices.size(); i++)
        callback(m_RenderObjects[indices[i]], i);
}

template <typename Callback>
void ModelCollection::IterateRenderObjects(const RenderObjectIndices& indices,
    const RenderObjectPermutation& permutation, Callback&& callback) const
{
    for (u32 i = 0; i < indices.size(); i++)
        callback(m_RenderObjects[indices[permutation[i]]], permutation[i]);
}

template <typename T>
RenderHandle<T> ModelCollection::AddRenderHandle(const T& object, RenderHandleArray<T>& array)
{
    return array.push_back(object);
}

