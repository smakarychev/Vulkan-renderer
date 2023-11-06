#pragma once

#include "Converters.h"

namespace utils
{
    assetLib::BoundingSphere welzlSphere(std::vector<glm::vec3> points);

    u32 nextPowerOf2(u32 number);
    
    void remapMesh(ModelConverter::MeshData& meshData, u32 maxTrianglesPerMeshlet);
    std::vector<assetLib::ModelInfo::Meshlet> createMeshlets(ModelConverter::MeshData& meshData);
}
