#pragma once

#include "Vulkan/VulkanInclude.h"

class Renderer;

// todo: rename
struct Vertex3D
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 Color;
    glm::vec2 UV;

    static VertexInputDescription GetInputDescription();
    bool operator==(const Vertex3D& other) const
    {
        return Position == other.Position && Normal == other.Normal && Color == other.Color && UV == other.UV;
    }
};

namespace std
{
    template<> struct hash<Vertex3D>
    {
        size_t operator()(const Vertex3D& vertex) const noexcept;
    };
}

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
    Mesh(const std::vector<Vertex3D>& vertices, const std::vector<u32>& indices);
    static Mesh LoadFromAsset(std::string_view path);
    void Upload(const Renderer& renderer);
    const Buffer& GetVertexBuffer() const { return m_VertexBuffer; }
    const Buffer& GetIndexBuffer() const { return m_IndexBuffer; }
    u32 GetVertexCount() const { return (u32)m_Vertices.size(); }
    u32 GetIndexCount() const { return (u32)m_Indices.size(); }
    const std::vector<Vertex3D>& GetVertices() const { return m_Vertices; }
private:
    std::vector<Vertex3D> m_Vertices;
    std::vector<u32> m_Indices;
    Buffer m_VertexBuffer;
    Buffer m_IndexBuffer;
};
