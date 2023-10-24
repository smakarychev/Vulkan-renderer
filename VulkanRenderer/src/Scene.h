#pragma once
#include <string>
#include <unordered_map>

#include "AssetLib.h"
#include "HandleArray.h"
#include "ModelAsset.h"
#include "RenderObject.h"
#include "Vulkan/Image.h"

class ResourceUploader;
class ShaderDescriptorSet;
class ShaderPipelineTemplate;
class Model;
struct RenderObject;
class Mesh;
struct Material;

// todo: WIP, name is temporary
struct SharedMeshContext
{
    Buffer Positions;
    Buffer Normals;
    Buffer UVs;
    Buffer Indices;
};

struct RenderObjectSSBO
{
    struct Data
    {
        glm::mat4 Transform;
        assetLib::BoundingSphere BoundingSphere;    
    };

    std::vector<Data> Objects{MAX_OBJECTS};
    Buffer Buffer;
};

class Scene
{
public:
    void OnInit();
    void OnShutdown();
    
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

    void CreateSharedMeshContext(ResourceUploader& resourceUploader);
    const Buffer& GetIndirectBuffer() const { return m_IndirectBuffer; }
    const Buffer& GetIndirectCompactBuffer() const { return m_IndirectCompactBuffer; }
    const Buffer& GetRenderObjectsBuffer() const { return m_RenderObjectSSBO.Buffer; }
    
    void AddRenderObject(const RenderObject& renderObject);
    bool IsDirty() const { return m_IsDirty; }
    void ClearDirty() { m_IsDirty = false; }
    
    void Bind(const CommandBuffer& cmd) const;
    
private:
    void ReleaseMeshSharedContext();
private:
    std::unordered_map<std::string, ShaderPipelineTemplate> m_ShaderTemplates;
    std::unordered_map<std::string, Model*> m_Models;
    HandleArray<Material> m_Materials;
    HandleArray<MaterialGPU> m_MaterialsGPU;
    HandleArray<Mesh> m_Meshes;
    HandleArray<Texture> m_Textures;

    std::vector<RenderObject> m_RenderObjects;

    std::unique_ptr<SharedMeshContext> m_SharedMeshContext;
    Buffer m_IndirectBuffer;
    Buffer m_IndirectCompactBuffer;
    RenderObjectSSBO m_RenderObjectSSBO{};
    
    bool m_IsDirty{false};
};
