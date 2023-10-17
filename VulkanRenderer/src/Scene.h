#pragma once
#include <string>
#include <unordered_map>

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
    MaterialGPU* GetMaterialBindless(const std::string& name);
    Material* GetMaterial(const std::string& name);
    Model* GetModel(const std::string& name);
    Mesh* GetMesh(const std::string& name);
    Texture* GetTexture(const std::string& name);

    void AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name);
    void AddMaterialGPU(const MaterialGPU& material, const std::string& name);
    void AddMaterial(const Material& material, const std::string& name);
    void AddMesh(const Mesh& mesh, const std::string& name);
    void AddModel(Model* model, const std::string& name);
    void AddTexture(const Texture& texture, const std::string& name);

    const std::vector<RenderObject>& GetRenderObjects() const { return m_RenderObjects; }
    std::vector<RenderObject>& GetRenderObjects() { return m_RenderObjects; }

    void UpdateRenderObject(ShaderDescriptorSet& bindlessDescriptorSet, BindlessDescriptorsState& bindlessDescriptorsState);
    
    void CreateIndirectBatches();
    const std::vector<BatchIndirect>& GetIndirectBatches() const { return m_IndirectBatches; }
    
    void AddRenderObject(const RenderObject& renderObject);
    bool IsDirty() const { return m_IsDirty; }
    void ClearDirty() { m_IsDirty = false; }
private:
    std::unordered_map<std::string, ShaderPipelineTemplate> m_ShaderTemplates;
    std::unordered_map<std::string, Material> m_Materials;
    std::unordered_map<std::string, MaterialGPU> m_MaterialsGPU;
    std::unordered_map<std::string, Model*> m_Models;
    std::unordered_map<std::string, Mesh> m_Meshes;
    std::unordered_map<std::string, Texture> m_Textures;

    std::vector<RenderObject> m_RenderObjects;
    std::vector<BatchIndirect> m_IndirectBatches;
    bool m_IsDirty{false};
    u32 m_NewRenderObjectsIndex{0};
};
