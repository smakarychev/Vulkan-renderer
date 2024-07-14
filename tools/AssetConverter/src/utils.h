#pragma once

#include "Converters.h"

namespace Utils
{
    void remapMesh(ModelConverter::MeshData& meshData, std::vector<u32>& indices);
    std::vector<assetLib::ModelInfo::Meshlet> createMeshlets(ModelConverter::MeshData& meshData,
        const std::vector<u32>& indices);

    struct BoundingVolumes
    {
        assetLib::BoundingSphere Sphere;
        assetLib::BoundingBox Box;
    };
    BoundingVolumes meshBoundingVolumes(const std::vector<assetLib::ModelInfo::Meshlet>& meshlets);
}
