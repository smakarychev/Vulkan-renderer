#pragma once

#include "ModelAsset.h"
#include "Vulkan/VulkanInclude.h"

class ResourceUploader;
class Renderer;

struct VertexP3N3T3UV2
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 Tangent;
    glm::vec2 UV;

    static VertexInputDescription GetInputDescription();
    static VertexInputDescription GetInputDescriptionDI();
};

using Meshlet = assetLib::ModelInfo::Meshlet;
using BoundingSphere = assetLib::BoundingSphere;

class Mesh
{
    using IndexType = assetLib::ModelInfo::IndexType;
public:
    Mesh(const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec3>& tangents,
        const std::vector<glm::vec2>& uvs, const std::vector<IndexType>& indices,
        const BoundingSphere& boundingSphere,
        const std::vector<Meshlet>& meshlets);

    u32 GetVertexCount() const { return (u32)m_Positions.size(); }
    u32 GetIndexCount() const { return (u32)m_Indices.size(); }

    const std::vector<glm::vec3>& GetPositions() const { return m_Positions; }
    const std::vector<glm::vec3>& GetNormals() const { return m_Normals; }
    const std::vector<glm::vec3>& GetTangents() const { return m_Tangents; }
    const std::vector<glm::vec2>& GetUVs() const { return m_UVs; }
    const std::vector<IndexType>& GetIndices() const { return m_Indices; }

    void SetVertexBufferOffset(i32 offset) { m_VertexBufferOffset = offset; }
    void SetIndexBufferOffset(u32 offset) { m_IndexBufferOffset = offset; }
    
    i32 GetVertexBufferOffset() const { return m_VertexBufferOffset; }
    u32 GetIndexBufferOffset() const { return m_IndexBufferOffset; }

    const BoundingSphere& GetBoundingSphere() const { return m_BoundingSphere; }
    const std::vector<Meshlet>& GetMeshlets() const { return m_Meshlets; }
private:
    std::vector<glm::vec3> m_Positions;
    std::vector<glm::vec3> m_Normals;
    std::vector<glm::vec3> m_Tangents;
    std::vector<glm::vec2> m_UVs;
    std::vector<IndexType> m_Indices;

    i32 m_VertexBufferOffset{};
    u32 m_IndexBufferOffset{};

    BoundingSphere m_BoundingSphere;
    std::vector<Meshlet> m_Meshlets;
};
