#pragma once

#include "ModelAsset.h"
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
    Mesh(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec2>& uvs, const std::vector<u32>& indices,
        const assetLib::BoundingSphere& boundingSphere);

    u32 GetVertexCount() const { return (u32)m_Positions.size(); }
    u32 GetIndexCount() const { return (u32)m_Indices.size(); }

    const std::vector<glm::vec3>& GetPositions() const { return m_Positions; }
    const std::vector<glm::vec3>& GetNormals() const { return m_Normals; }
    const std::vector<glm::vec2>& GetUVs() const { return m_UVs; }
    const std::vector<u32>& GetIndices() const { return m_Indices; }

    void SetVertexBufferOffset(i32 offset) { m_VertexBufferOffset = offset; }
    void SetIndexBufferOffset(u32 offset) { m_IndexBufferOffset = offset; }
    
    i32 GetVertexBufferOffset() const { return m_VertexBufferOffset; }
    u32 GetIndexBufferOffset() const { return m_IndexBufferOffset; }

    const assetLib::BoundingSphere& GetBoundingSphere() const { return m_BoundingSphere; }
private:
    std::vector<glm::vec3> m_Positions;
    std::vector<glm::vec3> m_Normals;
    std::vector<glm::vec2> m_UVs;
    std::vector<u32> m_Indices;

    i32 m_VertexBufferOffset;
    u32 m_IndexBufferOffset;

    assetLib::BoundingSphere m_BoundingSphere;
};
