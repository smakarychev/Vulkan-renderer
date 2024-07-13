#pragma once

#include "Converters.h"

namespace Utils
{
    void remapMesh(ModelConverter::MeshData& meshData, std::vector<u32>& indices);
    std::vector<assetLib::ModelInfo::Meshlet> createMeshlets(ModelConverter::MeshData& meshData,
        const std::vector<u32>& indices);
    assetLib::BoundingSphere meshBoundingSphere(const std::vector<assetLib::ModelInfo::Meshlet>& meshlets);
}
