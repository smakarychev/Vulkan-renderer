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

struct IndirectCommand
{
    VkDrawIndexedIndirectCommand VulkanCommand;
    RenderHandle<RenderObject> RenderObject;
};

struct MeshBatchBuffers
{
    Buffer IndicesCompact;
    Buffer TrianglesCompact;
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

struct RenderObjectVisibilitySSBO
{
    struct Data
    {
        u32 VisibilityFlags;
    };
    std::vector<Data> ObjectsVisibility{MAX_OBJECTS};
    Buffer Buffer;
};

struct MaterialDataSSBO
{
    Buffer Buffer;
    std::vector<MaterialGPU> Materials{MAX_OBJECTS};
};

struct MeshletsSSBO
{
    struct Data
    {
        assetLib::BoundingCone BoundingCone;
        assetLib::BoundingSphere BoundingSphere;
        u32 IsOccluded;
    };

    std::vector<Data> Meshlets{MAX_OBJECTS};
    Buffer Buffer;
};

class Scene
{
public:
    void OnInit(ResourceUploader* resourceUploader);
    void OnShutdown();

    void OnUpdate(f32 dt);
    
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

    void CreateSharedMeshContext();
    const Buffer& GetRenderObjectsBuffer() const { return m_RenderObjectSSBO.Buffer; }
    const Buffer& GetRenderObjectsVisibilityBuffer() const { return m_RenderObjectVisibilitySSBO.Buffer; }
    const Buffer& GetMaterialsBuffer() const { return m_MaterialDataSSBO.Buffer; }
    
    const Buffer& GetMeshletsIndirectBuffer() const { return m_MeshletsIndirectRawBuffer; }
    const Buffer& GetMeshletsIndirectFinalBuffer() const { return m_MeshletsIndirectFinalBuffer; }
    u32 GetMeshletCount() const { return m_MeshletCount; }

    const Buffer& GetMeshletsBuffer() const { return m_MeshletsSSBO.Buffer; }

    const Buffer& GetPositionsBuffer() const { return m_SharedMeshContext->Positions; }
    const Buffer& GetIndicesBuffer() const { return m_SharedMeshContext->Indices; }
    const Buffer& GetIndicesCompactBuffer() const { return m_MeshBatchBuffers.IndicesCompact; }
    const Buffer& GetTrianglesCompactBuffer() const { return m_MeshBatchBuffers.TrianglesCompact; }
    
    void AddRenderObject(const RenderObject& renderObject);
    bool IsDirty() const { return m_IsDirty; }
    void ClearDirty() { m_IsDirty = false; }
    
    void Bind(const CommandBuffer& cmd) const;
    
private:
    void ReleaseMeshSharedContext();
private:
    ResourceUploader* m_ResourceUploader{nullptr};
    
    std::unordered_map<std::string, ShaderPipelineTemplate> m_ShaderTemplates;
    std::unordered_map<std::string, Model*> m_Models;
    HandleArray<Material> m_Materials;
    HandleArray<MaterialGPU> m_MaterialsGPU;
    HandleArray<Mesh> m_Meshes;
    HandleArray<Texture> m_Textures;

    std::vector<RenderObject> m_RenderObjects;

    std::unique_ptr<SharedMeshContext> m_SharedMeshContext;
    MeshBatchBuffers m_MeshBatchBuffers;
    
    RenderObjectSSBO m_RenderObjectSSBO{};
    RenderObjectVisibilitySSBO m_RenderObjectVisibilitySSBO{};
    MaterialDataSSBO m_MaterialDataSSBO;

    Buffer m_MeshletsIndirectRawBuffer;
    Buffer m_MeshletsIndirectFinalBuffer;
    u32 m_MeshletCount{0};

    MeshletsSSBO m_MeshletsSSBO;

    u32 m_TotalTriangles{0};
    
    bool m_IsDirty{false};
};
