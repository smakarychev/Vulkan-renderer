#pragma once
#include <string>
#include <unordered_map>

#include "HandleArray.h"
#include "RenderObject.h"
#include "Vulkan/Image.h"

class ShaderDescriptorSet;
class ShaderPipelineTemplate;
class Model;
struct RenderObject;
class Mesh;
struct Material;

class Scene
{
public:
    ShaderPipelineTemplate* GetShaderTemplate(const std::string& name);
    Model* GetModel(const std::string& name);
    MaterialGPU& GetMaterialGPU(RenderHandle<MaterialGPU> handle);
    Mesh& GetMesh(RenderHandle<Mesh> handle);

    void AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name);
    void AddModel(Model* model, const std::string& name);
    RenderHandle<MaterialGPU> AddMaterialGPU(const MaterialGPU& material);
    RenderHandle<Material> AddMaterial(const Material& material);
    RenderHandle<Mesh> AddMesh(const Mesh& mesh);
    RenderHandle<Texture> AddTexture(const Texture& texture);

    const std::vector<RenderObject>& GetRenderObjects() const { return m_RenderObjects; }
    std::vector<RenderObject>& GetRenderObjects() { return m_RenderObjects; }

    void SetMaterialTexture(MaterialGPU& material, const Texture& texture, ShaderDescriptorSet& bindlessDescriptorSet,
        BindlessDescriptorsState& bindlessDescriptorsState);
    
    void CreateIndirectBatches();
    const std::vector<BatchIndirect>& GetIndirectBatches() const { return m_IndirectBatches; }
    
    void AddRenderObject(const RenderObject& renderObject);
    bool IsDirty() const { return m_IsDirty; }
    void ClearDirty() { m_IsDirty = false; }
private:
    std::unordered_map<std::string, ShaderPipelineTemplate> m_ShaderTemplates;
    std::unordered_map<std::string, Model*> m_Models;
    HandleArray<Material> m_Materials;
    HandleArray<MaterialGPU> m_MaterialsGPU;
    HandleArray<Mesh> m_Meshes;
    HandleArray<Texture> m_Textures;

    std::vector<RenderObject> m_RenderObjects;
    std::vector<BatchIndirect> m_IndirectBatches;
    bool m_IsDirty{false};
};
