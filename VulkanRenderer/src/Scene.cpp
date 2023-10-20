#include "Scene.h"

#include "Mesh.h"
#include "RenderObject.h"
#include "Model.h"
#include "ResourceUploader.h"
#include "Vulkan/Shader.h"

void Scene::OnInit()
{
    m_IndirectBuffer = Buffer::Builder()
       .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
       .SetSizeBytes(sizeof(VkDrawIndexedIndirectCommand) * MAX_DRAW_INDIRECT_CALLS)
       .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
       .Build();

    m_RenderObjectSSBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(m_RenderObjectSSBO.Objects.size() * sizeof(RenderObjectSSBO::Data))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();
}

void Scene::OnShutdown()
{
    if (m_SharedMeshContext)
        ReleaseMeshSharedContext();
}

ShaderPipelineTemplate* Scene::GetShaderTemplate(const std::string& name)
{
    auto it = m_ShaderTemplates.find(name);
    return it == m_ShaderTemplates.end() ? nullptr : &it->second;
}

Model* Scene::GetModel(const std::string& name)
{
    auto it = m_Models.find(name);
    return it == m_Models.end() ? nullptr : it->second;
}

MaterialGPU& Scene::GetMaterialGPU(RenderHandle<MaterialGPU> handle)
{
    return m_MaterialsGPU[handle];
}

Mesh& Scene::GetMesh(RenderHandle<Mesh> handle)
{
    return m_Meshes[handle];
}

void Scene::AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name)
{
    m_ShaderTemplates.emplace(std::make_pair(name, shaderTemplate));
}

void Scene::AddModel(Model* model, const std::string& name)
{
    m_Models.emplace(std::make_pair(name, model));
}

RenderHandle<MaterialGPU> Scene::AddMaterialGPU(const MaterialGPU& material)
{
    RenderHandle<MaterialGPU> handle = (u32)m_MaterialsGPU.size();
    m_MaterialsGPU.push_back(material);
    return handle;
}

RenderHandle<Material> Scene::AddMaterial(const Material& material)
{
    RenderHandle<Material> handle = (u32)m_Materials.size();
    m_Materials.push_back(material);
    return handle;
}

RenderHandle<Mesh> Scene::AddMesh(const Mesh& mesh)
{
    RenderHandle<Mesh> handle = (u32)m_Meshes.size();
    m_Meshes.push_back(mesh);
    return handle;
}

RenderHandle<Texture> Scene::AddTexture(const Texture& texture)
{
    RenderHandle<Texture> handle = (u32)m_Textures.size();
    m_Textures.push_back(texture);
    return handle;
}

void Scene::SetMaterialTexture(MaterialGPU& material, const Texture& texture,
    ShaderDescriptorSet& bindlessDescriptorSet, BindlessDescriptorsState& bindlessDescriptorsState)
{
    RenderHandle<Texture> albedoHandle = AddTexture(texture);
    Texture& albedoTexture = m_Textures[albedoHandle];
    material.AlbedoTextureHandle = bindlessDescriptorsState.TextureIndex;
    bindlessDescriptorSet.SetTexture("u_textures", albedoTexture, bindlessDescriptorsState.TextureIndex);
    bindlessDescriptorsState.TextureIndex++;
}

void Scene::CreateSharedMeshContext(ResourceUploader& resourceUploader)
{
    if (m_RenderObjects.empty())
        return;
    
    if (m_SharedMeshContext)
        ReleaseMeshSharedContext();

    m_SharedMeshContext = std::make_unique<SharedMeshContext>();

    u64 totalPositionsSizeBytes = 0;
    u64 totalNormalsSizeBytes = 0;
    u64 totalUVsSizeBytes = 0;
    u64 totalIndicesSizeBytes = 0;
    for (auto& mesh : m_Meshes)
    {
        totalPositionsSizeBytes += mesh.GetVertexCount() * sizeof(glm::vec3);
        totalNormalsSizeBytes += mesh.GetVertexCount() * sizeof(glm::vec3);
        totalUVsSizeBytes += mesh.GetVertexCount() * sizeof(glm::vec2);
        totalIndicesSizeBytes += mesh.GetIndexCount() * sizeof(u32);
    }

    Buffer::Builder vertexBufferBuilder = Buffer::Builder()
        .SetKinds({BufferKind::Vertex, BufferKind::Destination})
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    
    Buffer::Builder indexBufferBuilder = Buffer::Builder()
        .SetKinds({BufferKind::Index, BufferKind::Destination})
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    m_SharedMeshContext->Positions = vertexBufferBuilder
        .SetSizeBytes(totalPositionsSizeBytes)
        .BuildManualLifetime();

    m_SharedMeshContext->Normals = vertexBufferBuilder
        .SetSizeBytes(totalNormalsSizeBytes)
        .BuildManualLifetime();

    m_SharedMeshContext->UVs = vertexBufferBuilder
        .SetSizeBytes(totalUVsSizeBytes)
        .BuildManualLifetime();

    m_SharedMeshContext->Indices = indexBufferBuilder
        .SetSizeBytes(totalIndicesSizeBytes)
        .BuildManualLifetime();

    u64 verticesOffset = 0;
    u64 indicesOffset = 0;
    for (auto& mesh : m_Meshes)
    {
        u64 verticesSize = mesh.GetPositions().size();
        u64 indicesSize = mesh.GetIndices().size();
        resourceUploader.UpdateBuffer(m_SharedMeshContext->Positions, mesh.GetPositions().data(), verticesSize * sizeof(glm::vec3), verticesOffset * sizeof(glm::vec3));
        resourceUploader.UpdateBuffer(m_SharedMeshContext->Normals, mesh.GetNormals().data(), verticesSize * sizeof(glm::vec3), verticesOffset * sizeof(glm::vec3));
        resourceUploader.UpdateBuffer(m_SharedMeshContext->UVs, mesh.GetUVs().data(), verticesSize * sizeof(glm::vec2), verticesOffset * sizeof(glm::vec2));
        resourceUploader.UpdateBuffer(m_SharedMeshContext->Indices, mesh.GetIndices().data(), indicesSize * sizeof(u32), indicesOffset * sizeof(u32));
        
        mesh.SetVertexBufferOffset((i32)verticesOffset);
        mesh.SetIndexBufferOffset((u32)indicesOffset);
        
        verticesOffset += verticesSize;
        indicesOffset += indicesSize;
    }

    u32 mappedBuffer = resourceUploader.GetMappedBuffer(m_IndirectBuffer.GetSizeBytes());
    VkDrawIndexedIndirectCommand* commands = (VkDrawIndexedIndirectCommand*)resourceUploader.GetMappedAddress(mappedBuffer);
    for (u32 i = 0; i < m_RenderObjects.size(); i++)
    {
        const auto& object = m_RenderObjects[i];
        Mesh& mesh = GetMesh(object.Mesh);
        commands[i].firstIndex = mesh.GetIndexBufferOffset();
        commands[i].indexCount = mesh.GetIndexCount();
        commands[i].firstInstance = i;
        commands[i].instanceCount = 1;
        commands[i].vertexOffset = mesh.GetVertexBufferOffset();
    }
    resourceUploader.UpdateBuffer(m_IndirectBuffer, mappedBuffer, 0);

    // todo: this is not an optimal way to update this buffer
    for (u32 i = 0; i < m_RenderObjects.size(); i++)
    {
        const auto& object = m_RenderObjects[i];
        m_RenderObjectSSBO.Objects[i].Transform = object.Transform;
        m_RenderObjectSSBO.Objects[i].BoundingSphere = m_Meshes[object.Mesh].GetBoundingSphere();
    }
    resourceUploader.UpdateBuffer(m_RenderObjectSSBO.Buffer, m_RenderObjectSSBO.Objects.data(),
        m_RenderObjectSSBO.Objects.size() * sizeof(RenderObjectSSBO::Data), 0);
}

void Scene::AddRenderObject(const RenderObject& renderObject)
{
    m_RenderObjects.push_back(renderObject);
    m_IsDirty = true;
}

void Scene::Bind(const CommandBuffer& cmd) const
{
    RenderCommand::BindVertexBuffers(cmd, {m_SharedMeshContext->Positions, m_SharedMeshContext->Normals, m_SharedMeshContext->UVs}, {0, 0, 0});
    RenderCommand::BindIndexBuffer(cmd, m_SharedMeshContext->Indices, 0);
}

void Scene::ReleaseMeshSharedContext()
{
    Buffer::Destroy(m_SharedMeshContext->Positions);
    Buffer::Destroy(m_SharedMeshContext->Normals);
    Buffer::Destroy(m_SharedMeshContext->UVs);
    Buffer::Destroy(m_SharedMeshContext->Indices);
    
    m_SharedMeshContext.reset();
}
