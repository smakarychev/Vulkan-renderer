#include "Scene.h"

#include "Mesh.h"
#include "RenderObject.h"
#include "Model.h"
#include "ResourceUploader.h"
#include "Core/Random.h"
#include "Vulkan/Shader.h"

void Scene::OnInit(ResourceUploader* resourceUploader)
{
    m_ResourceUploader = resourceUploader;
    
    m_RenderObjectSSBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(m_RenderObjectSSBO.Objects.size() * sizeof(RenderObjectSSBO::Data))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_RenderObjectVisibilitySSBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(m_RenderObjectVisibilitySSBO.ObjectsVisibility.size() * sizeof(RenderObjectVisibilitySSBO::Data))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_MaterialDataSSBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(m_MaterialDataSSBO.Materials.size() * sizeof(MaterialGPU))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_MeshletsIndirectRawBuffer = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(sizeof(IndirectCommand) * MAX_DRAW_INDIRECT_CALLS)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_MeshletsIndirectCompactBuffer = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(sizeof(IndirectCommand) * MAX_DRAW_INDIRECT_CALLS)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();
    
    m_MeshletsIndirectFinalBuffer = Buffer::Builder()
        .SetKinds({BufferKind::Indirect, BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(sizeof(IndirectCommand) * MAX_DRAW_INDIRECT_CALLS)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_MeshletsSSBO.Buffer = Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(m_MeshletsSSBO.Meshlets.size() * sizeof(MeshletsSSBO::Data))
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    // todo: fix size to smaller number
    m_MeshBatchBuffers.IndicesCompact = Buffer::Builder()
        .SetKinds({BufferKind::Index, BufferKind::Storage, BufferKind::Destination})
        .SetSizeBytes(2llu * 256llu * 3llu * MAX_DRAW_INDIRECT_CALLS)
        .BuildManualLifetime();
    
    // todo: fix size to smaller number
    m_MeshBatchBuffers.TrianglesCompact =  Buffer::Builder()
        .SetKinds({BufferKind::Storage, BufferKind::Destination})
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .SetSizeBytes(4llu * 256llu * MAX_DRAW_INDIRECT_CALLS)
        .BuildManualLifetime();
}

void Scene::OnShutdown()
{
    if (m_SharedMeshContext)
        ReleaseMeshSharedContext();

    Buffer::Destroy(m_MeshBatchBuffers.IndicesCompact);
    Buffer::Destroy(m_MeshBatchBuffers.TrianglesCompact);
}

void Scene::OnUpdate(f32 dt)
{
    // assuming that object transform can change
    for (u32 i = 0; i < m_RenderObjects.size(); i++)
    {
        MaterialGPU& material = GetMaterialGPU(m_RenderObjects[i].MaterialGPU);
        m_MaterialDataSSBO.Materials[i].Albedo = material.Albedo;
        m_MaterialDataSSBO.Materials[i].AlbedoTextureHandle = material.AlbedoTextureHandle;
    }

    m_ResourceUploader->UpdateBuffer(m_MaterialDataSSBO.Buffer, m_MaterialDataSSBO.Materials.data(), m_MaterialDataSSBO.Materials.size() * sizeof(MaterialGPU), 0);
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

void Scene::CreateSharedMeshContext()
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
        totalIndicesSizeBytes += mesh.GetIndexCount() * sizeof(u16);
    }

    Buffer::Builder vertexBufferBuilder = Buffer::Builder()
        .SetKinds({BufferKind::Vertex, BufferKind::Storage, BufferKind::Destination})
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    
    Buffer::Builder indexBufferBuilder = Buffer::Builder()
        .SetKinds({BufferKind::Index, BufferKind::Storage, BufferKind::Destination})
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
        m_ResourceUploader->UpdateBuffer(m_SharedMeshContext->Positions, mesh.GetPositions().data(), verticesSize * sizeof(glm::vec3), verticesOffset * sizeof(glm::vec3));
        m_ResourceUploader->UpdateBuffer(m_SharedMeshContext->Normals, mesh.GetNormals().data(), verticesSize * sizeof(glm::vec3), verticesOffset * sizeof(glm::vec3));
        m_ResourceUploader->UpdateBuffer(m_SharedMeshContext->UVs, mesh.GetUVs().data(), verticesSize * sizeof(glm::vec2), verticesOffset * sizeof(glm::vec2));
        m_ResourceUploader->UpdateBuffer(m_SharedMeshContext->Indices, mesh.GetIndices().data(), indicesSize * sizeof(u16), indicesOffset * sizeof(u16));
        
        mesh.SetVertexBufferOffset((i32)verticesOffset);
        mesh.SetIndexBufferOffset((u32)indicesOffset);
        
        verticesOffset += verticesSize;
        indicesOffset += indicesSize;
    }

    // todo: this is not an optimal way to update this buffer
    for (u32 i = 0; i < m_RenderObjects.size(); i++)
    {
        const auto& object = m_RenderObjects[i];
        m_RenderObjectSSBO.Objects[i].Transform = object.Transform;
        m_RenderObjectSSBO.Objects[i].BoundingSphere = m_Meshes[object.Mesh].GetBoundingSphere();
    }
    m_ResourceUploader->UpdateBuffer(m_RenderObjectSSBO.Buffer, m_RenderObjectSSBO.Objects.data(),
        m_RenderObjectSSBO.Objects.size() * sizeof(RenderObjectSSBO::Data), 0);

    // meshlets *******************************************************************************

    u32 mappedBuffer = m_ResourceUploader->GetMappedBuffer(m_MeshletsIndirectRawBuffer.GetSizeBytes());
    IndirectCommand* commands = (IndirectCommand*)m_ResourceUploader->GetMappedAddress(mappedBuffer);
    m_MeshletCount = 0;
    for (u32 i = 0; i < m_RenderObjects.size(); i++)
    {
        const auto& object = m_RenderObjects[i];
        Mesh& mesh = GetMesh(object.Mesh);
        for (auto& meshlet : mesh.GetMeshlets())
        {
            commands[m_MeshletCount].VulkanCommand.firstIndex = mesh.GetIndexBufferOffset() + meshlet.FirstIndex;
            commands[m_MeshletCount].VulkanCommand.indexCount = meshlet.IndexCount;
            commands[m_MeshletCount].VulkanCommand.firstInstance = m_MeshletCount;
            commands[m_MeshletCount].VulkanCommand.instanceCount = 1;
            commands[m_MeshletCount].VulkanCommand.vertexOffset = mesh.GetVertexBufferOffset() + (i32)meshlet.FirstVertex;
            commands[m_MeshletCount].RenderObject = i;
            m_MeshletCount++;
            m_TotalTriangles += meshlet.IndexCount / 3;
        }
    }
    m_ResourceUploader->UpdateBuffer(m_MeshletsIndirectRawBuffer, mappedBuffer, 0);

    m_MeshletCount = 0;
    for (u32 i = 0; i < m_RenderObjects.size(); i++)
    {
        const auto& object = m_RenderObjects[i];
        Mesh& mesh = GetMesh(object.Mesh);
        for (auto& meshlet : mesh.GetMeshlets())
        {
            m_MeshletsSSBO.Meshlets[m_MeshletCount].BoundingCone = meshlet.BoundingCone;
            m_MeshletsSSBO.Meshlets[m_MeshletCount].BoundingSphere = meshlet.BoundingSphere;
            m_MeshletCount++;
        }
    }
    m_ResourceUploader->UpdateBuffer(m_MeshletsSSBO.Buffer, m_MeshletsSSBO.Meshlets.data(),
        m_MeshletsSSBO.Meshlets.size() * sizeof(MeshletsSSBO::Data), 0);
}

void Scene::AddRenderObject(const RenderObject& renderObject)
{
    m_RenderObjects.push_back(renderObject);
    m_IsDirty = true;
}

void Scene::Bind(const CommandBuffer& cmd) const
{
    RenderCommand::BindVertexBuffers(cmd, {m_SharedMeshContext->Positions, m_SharedMeshContext->Normals, m_SharedMeshContext->UVs}, {0, 0, 0});
    RenderCommand::BindIndexBuffer(cmd, m_MeshBatchBuffers.IndicesCompact, 0);
}

void Scene::ReleaseMeshSharedContext()
{
    Buffer::Destroy(m_SharedMeshContext->Positions);
    Buffer::Destroy(m_SharedMeshContext->Normals);
    Buffer::Destroy(m_SharedMeshContext->UVs);
    Buffer::Destroy(m_SharedMeshContext->Indices);
    
    m_SharedMeshContext.reset();
}
