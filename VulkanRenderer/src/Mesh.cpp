#include "Mesh.h"

#include <glm/gtx/hash.hpp>

#include "Renderer.h"
#include "utils.h"

VertexInputDescription VertexP3N3UV2::GetInputDescription()
{
    VertexInputDescription inputDescription = {};
    inputDescription.Bindings.reserve(1);
    inputDescription.Attributes.reserve(3);

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = sizeof(VertexP3N3UV2);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    inputDescription.Bindings.push_back(binding);

    VkVertexInputAttributeDescription position = {};
    position.binding = 0;
    position.location = 0;
    position.format = VK_FORMAT_R32G32B32_SFLOAT;
    position.offset = offsetof(VertexP3N3UV2, Position);

    VkVertexInputAttributeDescription normal = {};
    normal.binding = 0;
    normal.location = 1;
    normal.format = VK_FORMAT_R32G32B32_SFLOAT;
    normal.offset = offsetof(VertexP3N3UV2, Normal);
    
    VkVertexInputAttributeDescription uv = {};
    uv.binding = 0;
    uv.location = 2;
    uv.format = VK_FORMAT_R32G32_SFLOAT;
    uv.offset = offsetof(VertexP3N3UV2, UV);

    inputDescription.Attributes.push_back(position);
    inputDescription.Attributes.push_back(normal);
    inputDescription.Attributes.push_back(uv);

    return inputDescription;
}

VertexInputDescription VertexP3N3UV2::GetInputDescriptionDI()
{
    VertexInputDescription inputDescription = {};
    inputDescription.Bindings.reserve(3);
    inputDescription.Attributes.reserve(3);

    VkVertexInputBindingDescription positionBinding = {};
    positionBinding.binding = 0;
    positionBinding.stride = sizeof(glm::vec3);
    positionBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputBindingDescription normalBinding = {};
    normalBinding.binding = 1;
    normalBinding.stride = sizeof(glm::vec3);
    normalBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputBindingDescription uvBinding = {};
    uvBinding.binding = 2;
    uvBinding.stride = sizeof(glm::vec2);
    uvBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription position = {};
    position.binding = 0;
    position.location = 0;
    position.format = VK_FORMAT_R32G32B32_SFLOAT;
    position.offset = 0;

    VkVertexInputAttributeDescription normal = {};
    normal.binding = 1;
    normal.location = 1;
    normal.format = VK_FORMAT_R32G32B32_SFLOAT;
    normal.offset = 0;
    
    VkVertexInputAttributeDescription uv = {};
    uv.binding = 2;
    uv.location = 2;
    uv.format = VK_FORMAT_R32G32_SFLOAT;
    uv.offset = 0;

    inputDescription.Bindings.push_back(positionBinding);
    inputDescription.Bindings.push_back(normalBinding);
    inputDescription.Bindings.push_back(uvBinding);

    inputDescription.Attributes.push_back(position);
    inputDescription.Attributes.push_back(normal);
    inputDescription.Attributes.push_back(uv);

    return inputDescription;
}

PushConstantBuffer::PushConstantBuffer()
{
    m_Description = PushConstantDescription::Builder()
        .SetStages(VK_SHADER_STAGE_VERTEX_BIT)
        .SetSizeBytes(sizeof(m_MeshPushConstants))
        .Build();
}

Mesh::Mesh(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec2>& uvs, const std::vector<u32>& indices)
    : m_Positions(positions), m_Normals(normals), m_UVs(uvs), m_Indices(indices)
{
    u64 positionsSizeBytes = positions.size() * sizeof(glm::vec3);
    u64 normalsSizeBytes = normals.size() * sizeof(glm::vec3);
    u64 uvsSizeBytes = uvs.size() * sizeof(glm::vec2);
    u64 indicesSizeBytes = indices.size() * sizeof(u32);

    m_PositionsBuffer = Buffer::Builder()
        .SetKinds({BufferKind::Vertex, BufferKind::Destination})
        .SetSizeBytes(positionsSizeBytes)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_NormalsBuffer = Buffer::Builder()
        .SetKinds({BufferKind::Vertex, BufferKind::Destination})
        .SetSizeBytes(normalsSizeBytes)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_UVsBuffer = Buffer::Builder()
        .SetKinds({BufferKind::Vertex, BufferKind::Destination})
        .SetSizeBytes(uvsSizeBytes)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();

    m_IndexBuffer = Buffer::Builder()
        .SetKinds({BufferKind::Index, BufferKind::Destination})
        .SetSizeBytes(indicesSizeBytes)
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
        .Build();
}

void Mesh::Upload(const Renderer& renderer)
{
    {
        Buffer stageBuffer = Buffer::Builder()
            .SetKind(BufferKind::Source)
            .SetSizeBytes(m_PositionsBuffer.GetSizeBytes())
            .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
            .BuildManualLifetime();

        stageBuffer.SetData(m_Positions.data(), m_PositionsBuffer.GetSizeBytes());

        renderer.ImmediateUpload([&](const CommandBuffer& cmd)
        {
            RenderCommand::CopyBuffer(cmd, stageBuffer, m_PositionsBuffer, {stageBuffer.GetSizeBytes(), 0, 0});
        });
    
        Buffer::Destroy(stageBuffer);
    }

    {
        Buffer stageBuffer = Buffer::Builder()
            .SetKind(BufferKind::Source)
            .SetSizeBytes(m_NormalsBuffer.GetSizeBytes())
            .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
            .BuildManualLifetime();

        stageBuffer.SetData(m_Normals.data(), m_NormalsBuffer.GetSizeBytes());

        renderer.ImmediateUpload([&](const CommandBuffer& cmd)
        {
            RenderCommand::CopyBuffer(cmd, stageBuffer, m_NormalsBuffer, {stageBuffer.GetSizeBytes(), 0, 0});
        });
    
        Buffer::Destroy(stageBuffer);
    }

    {
        Buffer stageBuffer = Buffer::Builder()
            .SetKind(BufferKind::Source)
            .SetSizeBytes(m_UVsBuffer.GetSizeBytes())
            .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
            .BuildManualLifetime();

        stageBuffer.SetData(m_UVs.data(), m_UVsBuffer.GetSizeBytes());

        renderer.ImmediateUpload([&](const CommandBuffer& cmd)
        {
            RenderCommand::CopyBuffer(cmd, stageBuffer, m_UVsBuffer, {stageBuffer.GetSizeBytes(), 0, 0});
        });
    
        Buffer::Destroy(stageBuffer);
    }

    {
        Buffer stageBuffer = Buffer::Builder()
        .SetKind(BufferKind::Source)
        .SetSizeBytes(m_IndexBuffer.GetSizeBytes())
        .SetMemoryFlags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
        .BuildManualLifetime();

        stageBuffer.SetData(m_Indices.data(), m_IndexBuffer.GetSizeBytes());

        renderer.ImmediateUpload([&](const CommandBuffer& cmd)
        {
            RenderCommand::CopyBuffer(cmd, stageBuffer, m_IndexBuffer, {stageBuffer.GetSizeBytes(), 0, 0});
        });
    
        Buffer::Destroy(stageBuffer);
    }
}

void Mesh::Bind(const CommandBuffer& cmd) const
{
    RenderCommand::BindVertexBuffers(cmd, {m_PositionsBuffer, m_NormalsBuffer, m_UVsBuffer}, {0, 0, 0});
    RenderCommand::BindIndexBuffer(cmd, m_IndexBuffer, 0);
}
