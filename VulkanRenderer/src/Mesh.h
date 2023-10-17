#pragma once

#include "Vulkan/VulkanInclude.h"

class ResourceUploader;
class Renderer;

struct VertexP3N3UV2
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 UV;

    static VertexInputDescription GetInputDescription();
    static VertexInputDescription GetInputDescriptionDI();
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
    Mesh(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& uvs, const std::vector<u32>& indices);
    void Upload(ResourceUploader& uploader);
    u32 GetVertexCount() const { return (u32)m_Positions.size(); }
    u32 GetIndexCount() const { return (u32)m_Indices.size(); }
    void Bind(const CommandBuffer& cmd) const;
private:
    std::vector<glm::vec3> m_Positions;
    std::vector<glm::vec3> m_Normals;
    std::vector<glm::vec2> m_UVs;
    std::vector<u32> m_Indices;
    Buffer m_PositionsBuffer;
    Buffer m_NormalsBuffer;
    Buffer m_UVsBuffer;
    Buffer m_IndexBuffer;
};
