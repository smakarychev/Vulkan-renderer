﻿#pragma once

#include "Vulkan/VulkanInclude.h"

class Renderer;

struct VertexP3N3UV
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 UV;

    static VertexInputDescription GetInputDescription();
};

struct MeshPushConstants
{
    glm::vec4 Data;
    glm::mat4 Transform;
};

// todo: generic templated buffer?
class PushConstantBuffer
{
public:
    PushConstantBuffer();
    void SetData(const MeshPushConstants& pushConstants) { m_MeshPushConstants = pushConstants; }
    const MeshPushConstants& GetData() const { return m_MeshPushConstants; }
    MeshPushConstants& GetData() { return m_MeshPushConstants; }
    const PushConstantDescription& GetDescription() const { return m_Description; }
private:
    MeshPushConstants m_MeshPushConstants{};
    PushConstantDescription m_Description;
};

class Mesh
{
public:
    Mesh(const std::vector<VertexP3N3UV>& vertices, const std::vector<u32>& indices);
    void Upload(const Renderer& renderer);
    const Buffer& GetVertexBuffer() const { return m_VertexBuffer; }
    const Buffer& GetIndexBuffer() const { return m_IndexBuffer; }
    u32 GetVertexCount() const { return (u32)m_Vertices.size(); }
    u32 GetIndexCount() const { return (u32)m_Indices.size(); }
    const std::vector<VertexP3N3UV>& GetVertices() const { return m_Vertices; }
private:
    std::vector<VertexP3N3UV> m_Vertices;
    std::vector<u32> m_Indices;
    Buffer m_VertexBuffer;
    Buffer m_IndexBuffer;
};