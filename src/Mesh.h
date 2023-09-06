#pragma once

#include "Vulkan/VulkanInclude.h"

#include <glm/glm.hpp>

// todo: rename
struct Vertex3D
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 Color;

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
    Mesh(const std::vector<Vertex3D>& vertices);
    static Mesh LoadFromFile(std::string_view filePath);
    void Upload();
    const Buffer& GetBuffer() const { return m_Buffer; }
    u32 GetVertexCount() const { return (u32)m_Vertices.size(); }
private:
    std::vector<Vertex3D> m_Vertices;
    Buffer m_Buffer;
};
