﻿#include "Mesh.h"

#include <tiny_obj_loader.h>

#include "Renderer.h"

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

    inputDescription.Attributes.push_back(position);
    inputDescription.Attributes.push_back(normal);
    inputDescription.Attributes.push_back(color);

    return inputDescription;
}

PushConstantBuffer::PushConstantBuffer()
{
    m_Description = PushConstantDescription::Builder().
        SetStages(VK_SHADER_STAGE_VERTEX_BIT).
        SetSizeBytes(sizeof(m_MeshPushConstants)).
        Build();
}

Mesh::Mesh(const std::vector<Vertex3D>& vertices)
    : m_Vertices(vertices)
{
    u64 sizeBytes = vertices.size() * sizeof(Vertex3D);

    m_Buffer = Buffer::Builder().
        SetKinds({BufferKind::Vertex, BufferKind::Destination}).
        SetSizeBytes(sizeBytes).
        SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY).
        Build();
}

Mesh Mesh::LoadFromFile(std::string_view filePath)
{
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warnings, errors;

    bool success = tinyobj::LoadObj(&attributes, &shapes, &materials, &warnings, &errors, filePath.data());
    ASSERT(success, "Failed to load model: {}\nErrors:\n{}\nWarnings:\n{}", filePath, errors, warnings)

    if (!warnings.empty())
        LOG("Model load warnings ({}):\n{}", filePath, warnings);

    std::vector<Vertex3D> vertices;
    
    for (auto& shape : shapes)
    {
        for (auto& index : shape.mesh.indices)
        {
            Vertex3D vertex;
            
            vertex.Position[0] = attributes.vertices[3 * index.vertex_index + 0]; 
            vertex.Position[1] = attributes.vertices[3 * index.vertex_index + 1]; 
            vertex.Position[2] = attributes.vertices[3 * index.vertex_index + 2];

            vertex.Normal[0] = attributes.normals[3 * index.normal_index + 0]; 
            vertex.Normal[1] = attributes.normals[3 * index.normal_index + 1]; 
            vertex.Normal[2] = attributes.normals[3 * index.normal_index + 2];

            vertex.Color = vertex.Normal;
            
            vertices.push_back(vertex);
        }
    }
    
    return Mesh(vertices);
}

void Mesh::Upload()
{
    
}