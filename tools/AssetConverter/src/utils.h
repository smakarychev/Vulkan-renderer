#pragma once

#include "Converters.h"

namespace Utils
{
    struct Attributes
    {
        std::vector<glm::vec3>* Positions{nullptr};
        std::vector<glm::vec3>* Normals{nullptr};
        std::vector<glm::vec3>* Tangents{nullptr};
        std::vector<glm::vec2>* UVs{nullptr};
    };
    void remapMesh(Attributes& attributes, std::vector<u32>& indices);
    struct MeshletInfo
    {
        std::vector<assetLib::ModelInfo::Meshlet> Meshlets;
        std::vector<assetLib::ModelInfo::IndexType> Indices;
    };
    MeshletInfo createMeshlets(Attributes& attributes, const std::vector<u32>& indices);

    struct BoundingVolumes
    {
        assetLib::BoundingSphere Sphere;
        assetLib::BoundingBox Box;
    };
    BoundingVolumes meshBoundingVolumes(const std::vector<assetLib::ModelInfo::Meshlet>& meshlets);
}
