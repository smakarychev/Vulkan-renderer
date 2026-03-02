#pragma once

#include <AssetLib/Scenes/SceneAsset.h>

namespace utils
{
struct Attributes
{
    std::vector<glm::vec3>* Positions{nullptr};
    std::vector<glm::vec3>* Normals{nullptr};
    std::vector<glm::vec4>* Tangents{nullptr};
    std::vector<glm::vec2>* UVs{nullptr};
};

void remapMesh(Attributes& attributes, std::vector<u32>& indices);

struct MeshletInfo
{
    std::vector<lux::assetlib::SceneAssetMeshlet> Meshlets;
    std::vector<lux::assetlib::SceneAssetIndexType> Indices;
};

MeshletInfo createMeshlets(Attributes& attributes, const std::vector<u32>& indices);

struct BoundingVolumes
{
    Sphere Sphere;
    AABB Box;
};

BoundingVolumes meshBoundingVolumes(const std::vector<lux::assetlib::SceneAssetMeshlet>& meshlets);
}
