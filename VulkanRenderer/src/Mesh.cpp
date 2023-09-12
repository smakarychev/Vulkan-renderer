#include "Mesh.h"

#include <glm/gtx/hash.hpp>

#include "AssetLib.h"
#include "MeshAsset.h"
#include "Renderer.h"
#include "utils.h"

size_t std::hash<Vertex3D>::operator()(const Vertex3D& vertex) const noexcept
{
    auto&& [p, n, c, uv] = vertex;
    u64 hash = 0;
    utils::hashCombine(hash, vertex.Position);
    utils::hashCombine(hash, vertex.Normal);
    utils::hashCombine(hash, vertex.Color);
    utils::hashCombine(hash, vertex.UV);
    return hash;
}

VertexInputDescription Vertex3D::GetInputDescription()
{
    VertexInputDescription inputDescription = {};
    inputDescription.Bindings.reserve(1);
    inputDescription.Attributes.reserve(3);

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = sizeof(Vertex3D);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    inputDescription.Bindings.push_back(binding);

    VkVertexInputAttributeDescription position = {};
    position.binding = 0;
    position.location = 0;
    position.format = VK_FORMAT_R32G32B32_SFLOAT;
    position.offset = offsetof(Vertex3D, Position);

    VkVertexInputAttributeDescription normal = {};
    normal.binding = 0;
    normal.location = 1;
    normal.format = VK_FORMAT_R32G32B32_SFLOAT;
    normal.offset = offsetof(Vertex3D, Normal);
    
    VkVertexInputAttributeDescription color = {};
    color.binding = 0;
    color.location = 2;
    color.format = VK_FORMAT_R32G32B32_SFLOAT;
    color.offset = offsetof(Vertex3D, Color);

    VkVertexInputAttributeDescription uv = {};
    uv.binding = 0;
    uv.location = 3;
    uv.format = VK_FORMAT_R32G32_SFLOAT;
    uv.offset = offsetof(Vertex3D, UV);

    inputDescription.Attributes.push_back(position);
    inputDescription.Attributes.push_back(normal);
    inputDescription.Attributes.push_back(color);
    inputDescription.Attributes.push_back(uv);

    return inputDescription;
}

PushConstantBuffer::PushConstantBuffer()
{
    m_Description = PushConstantDescription::Builder().
        SetStages(VK_SHADER_STAGE_VERTEX_BIT).
        SetSizeBytes(sizeof(m_MeshPushConstants)).
        Build();
}

Mesh::Mesh(const std::vector<Vertex3D>& vertices, const std::vector<u32>& indices)
    : m_Vertices(vertices), m_Indices(indices)
{
    u64 verticesSizeBytes = vertices.size() * sizeof(Vertex3D);
    u64 indicesSizeBytes = indices.size() * sizeof(u32);

    m_VertexBuffer = Buffer::Builder().
        SetKinds({BufferKind::Vertex, BufferKind::Destination}).
        SetSizeBytes(verticesSizeBytes).
        SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT).
        Build();

    m_IndexBuffer = Buffer::Builder().
        SetKinds({BufferKind::Index, BufferKind::Destination}).
        SetSizeBytes(indicesSizeBytes).
        SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT).
        Build();
}

Mesh Mesh::LoadFromAsset(std::string_view path)
{
    assetLib::File meshFile;
    assetLib::loadBinaryFile(path, meshFile);
    assetLib::MeshInfo meshInfo = assetLib::readMeshInfo(meshFile);
    ASSERT(meshInfo.VertexFormat == assetLib::VertexFormat::P3N3C3UV2, "Unsupported vertex format")

    std::vector<Vertex3D> vertices(meshInfo.VerticesSizeBytes);
    std::vector<u32> indices(meshInfo.IndicesSizeBytes);

    //todo: very dangerous casts here
    assetLib::unpackMesh(meshInfo, meshFile.Blob.data(), meshFile.Blob.size(), (u8*)vertices.data(), (u8*)indices.data());

    return Mesh(vertices, indices);
}

void Mesh::Upload(const Renderer& renderer)
{
    {
        Buffer stageBuffer = Buffer::Builder().
        SetKind(BufferKind::Source).
        SetSizeBytes(m_VertexBuffer.GetSizeBytes()).
        SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT).
        BuildManualLifetime();

        stageBuffer.SetData(m_Vertices.data(), m_VertexBuffer.GetSizeBytes());

        renderer.ImmediateUpload([&](const CommandBuffer& cmd)
        {
            RenderCommand::CopyBuffer(cmd, stageBuffer, m_VertexBuffer);
        });
    
        Buffer::Destroy(stageBuffer);
    }

    {
        Buffer stageBuffer = Buffer::Builder().
        SetKind(BufferKind::Source).
        SetSizeBytes(m_IndexBuffer.GetSizeBytes()).
        SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT).
        BuildManualLifetime();

        stageBuffer.SetData(m_Indices.data(), m_IndexBuffer.GetSizeBytes());

        renderer.ImmediateUpload([&](const CommandBuffer& cmd)
        {
            RenderCommand::CopyBuffer(cmd, stageBuffer, m_IndexBuffer);
        });
    
        Buffer::Destroy(stageBuffer);
    }
}
