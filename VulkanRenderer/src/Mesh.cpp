#include "Mesh.h"

#include <glm/gtx/hash.hpp>

#include "Renderer.h"
#include "utils.h"

VertexInputDescription VertexP3N3T3UV2::GetInputDescription()
{
    VertexInputDescription inputDescription = {};
    inputDescription.Bindings.reserve(1);
    inputDescription.Attributes.reserve(3);

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = sizeof(VertexP3N3T3UV2);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    inputDescription.Bindings.push_back(binding);

    VkVertexInputAttributeDescription position = {};
    position.binding = 0;
    position.location = 0;
    position.format = VK_FORMAT_R32G32B32_SFLOAT;
    position.offset = offsetof(VertexP3N3T3UV2, Position);

    VkVertexInputAttributeDescription normal = {};
    normal.binding = 0;
    normal.location = 1;
    normal.format = VK_FORMAT_R32G32B32_SFLOAT;
    normal.offset = offsetof(VertexP3N3T3UV2, Normal);
    
    VkVertexInputAttributeDescription tangent = {};
    normal.binding = 0;
    normal.location = 2;
    normal.format = VK_FORMAT_R32G32B32_SFLOAT;
    normal.offset = offsetof(VertexP3N3T3UV2, Tangent);
    
    VkVertexInputAttributeDescription uv = {};
    uv.binding = 0;
    uv.location = 3;
    uv.format = VK_FORMAT_R32G32_SFLOAT;
    uv.offset = offsetof(VertexP3N3T3UV2, UV);

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

    VkVertexInputBindingDescription positionBinding = {};
    positionBinding.binding = 0;
    positionBinding.stride = sizeof(glm::vec3);
    positionBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputBindingDescription normalBinding = {};
    normalBinding.binding = 1;
    normalBinding.stride = sizeof(glm::vec3);
    normalBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkVertexInputBindingDescription tangentBinding = {};
    tangentBinding.binding = 2;
    tangentBinding.stride = sizeof(glm::vec3);
    tangentBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputBindingDescription uvBinding = {};
    uvBinding.binding = 3;
    uvBinding.stride = sizeof(glm::vec2);
    uvBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription position = {};
    position.binding = positionBinding.binding;
    position.location = positionBinding.binding;
    position.format = VK_FORMAT_R32G32B32_SFLOAT;
    position.offset = 0;

    VkVertexInputAttributeDescription normal = {};
    normal.binding = normalBinding.binding;
    normal.location = normalBinding.binding;
    normal.format = VK_FORMAT_R32G32B32_SFLOAT;
    normal.offset = 0;

    VkVertexInputAttributeDescription tangent = {};
    tangent.binding = tangentBinding.binding;
    tangent.location = tangentBinding.binding;
    tangent.format = VK_FORMAT_R32G32B32_SFLOAT;
    tangent.offset = 0;
    
    VkVertexInputAttributeDescription uv = {};
    uv.binding = uvBinding.binding;
    uv.location = uvBinding.binding;
    uv.format = VK_FORMAT_R32G32_SFLOAT;
    uv.offset = 0;

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
