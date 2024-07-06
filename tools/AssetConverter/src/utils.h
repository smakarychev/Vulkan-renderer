#pragma once

#include "Converters.h"

namespace Utils
{
    assetLib::BoundingSphere welzlSphere(std::vector<glm::vec3> points);

    u32 nextPowerOf2(u32 number);
    
    void remapMesh(ModelConverter::MeshData& meshData, std::vector<u32>& indices);
    std::vector<assetLib::ModelInfo::Meshlet> createMeshlets(ModelConverter::MeshData& meshData, std::vector<u32>& indices);
}
