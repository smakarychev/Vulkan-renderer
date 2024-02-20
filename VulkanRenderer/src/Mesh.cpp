#include "Mesh.h"

#include <glm/gtx/hash.hpp>

#include "Renderer.h"

VertexInputDescription VertexP3N3T3UV2::GetInputDescription()
{
    VertexInputDescription inputDescription = {};
    inputDescription.Bindings.reserve(1);
    inputDescription.Attributes.reserve(3);

    VertexInputDescription::Binding binding = {
        .Index = 0,
        .StrideBytes = sizeof(VertexP3N3T3UV2)};
    
    
    inputDescription.Bindings.push_back(binding);

    VertexInputDescription::Attribute position = {};
    position.BindingIndex = 0;
    position.Index = 0;
    position.Format = Format::RGB32_FLOAT;
    position.OffsetBytes = offsetof(VertexP3N3T3UV2, Position);

    VertexInputDescription::Attribute normal = {};
    normal.BindingIndex = 0;
    normal.Index = 1;
    normal.Format = Format::RGB32_FLOAT;
    normal.OffsetBytes = offsetof(VertexP3N3T3UV2, Normal);
    
    VertexInputDescription::Attribute tangent = {};
    tangent.BindingIndex = 0;
    tangent.Index = 2;
    tangent.Format = Format::RGB32_FLOAT;
    tangent.OffsetBytes = offsetof(VertexP3N3T3UV2, Tangent);
    
    VertexInputDescription::Attribute uv = {};
    uv.BindingIndex = 0;
    uv.Index = 3;
    uv.Format = Format::RG32_FLOAT;
    uv.OffsetBytes = offsetof(VertexP3N3T3UV2, UV);

    inputDescription.Attributes.push_back(position);
    inputDescription.Attributes.push_back(normal);
    inputDescription.Attributes.push_back(tangent);
    inputDescription.Attributes.push_back(uv);

    return inputDescription;
}

VertexInputDescription VertexP3N3T3UV2::GetInputDescriptionDI()
{
    VertexInputDescription inputDescription = {};
    inputDescription.Bindings.reserve(3);
    inputDescription.Attributes.reserve(3);

    VertexInputDescription::Binding positionBinding = {};
    positionBinding.Index = 0;
    positionBinding.StrideBytes = sizeof(glm::vec3);

    VertexInputDescription::Binding normalBinding = {};
    normalBinding.Index = 1;
    normalBinding.StrideBytes = sizeof(glm::vec3);
    
    VertexInputDescription::Binding tangentBinding = {};
    tangentBinding.Index = 2;
    tangentBinding.StrideBytes = sizeof(glm::vec3);

    VertexInputDescription::Binding uvBinding = {};
    uvBinding.Index = 3;
    uvBinding.StrideBytes = sizeof(glm::vec2);

    VertexInputDescription::Attribute position = {};
    position.BindingIndex = positionBinding.Index;
    position.Index = 0;
    position.Format = Format::RGB32_FLOAT;
    position.OffsetBytes = 0;

    VertexInputDescription::Attribute normal = {};
    normal.BindingIndex = normalBinding.Index;
    normal.Index = 0;
    normal.Format = Format::RGB32_FLOAT;
    normal.OffsetBytes = 0;

    VertexInputDescription::Attribute tangent = {};
    tangent.BindingIndex = tangentBinding.Index;
    tangent.Index = 0;
    tangent.Format = Format::RGB32_FLOAT;
    tangent.OffsetBytes = 0;
    
    VertexInputDescription::Attribute uv = {};
    uv.BindingIndex = uvBinding.Index;
    uv.Index = 0;
    uv.Format = Format::RG32_FLOAT;
    uv.OffsetBytes = 0;

    inputDescription.Bindings.push_back(positionBinding);
    inputDescription.Bindings.push_back(normalBinding);
    inputDescription.Bindings.push_back(tangentBinding);
    inputDescription.Bindings.push_back(uvBinding);

    inputDescription.Attributes.push_back(position);
    inputDescription.Attributes.push_back(normal);
    inputDescription.Attributes.push_back(tangent);
    inputDescription.Attributes.push_back(uv);

    return inputDescription;
}

Mesh::Mesh(const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec3>& tangents,
        const std::vector<glm::vec2>& uvs, const std::vector<IndexType>& indices,
        const BoundingSphere& boundingSphere,
        const std::vector<Meshlet>& meshlets)
    : m_Positions(positions), m_Normals(normals), m_Tangents(tangents), m_UVs(uvs), m_Indices(indices),
    m_BoundingSphere(boundingSphere),
    m_Meshlets(meshlets)
{}
