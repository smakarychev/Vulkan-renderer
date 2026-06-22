#pragma once

#include <AssetLib/Scenes/Scene/SceneAsset.h>
#include <meshoptimizer.h>

namespace utils
{
struct Attributes
{
    std::vector<glm::vec3> Positions{};
    std::vector<glm::vec3> Normals{};
    std::vector<glm::vec4> Tangents{};
    std::vector<glm::vec2> UVs{};
    std::vector<glm::u16vec4> Joints{};
    std::vector<glm::vec4> Weights{};
    
    void Init(u32 vertexCount);
};
struct RemapContext
{
    u32 InitialVertexCount{};
    u32 RemappedVertexCount{};
    std::vector<u32> InitialIndexRemap{};
    std::vector<u32> FinalIndexRemap{};
    std::vector<meshopt_Meshlet> MeshoptMeshlets;
    std::vector<u32> MeshoptMeshletsVertices;
};

void remapMesh(RemapContext& ctx, Attributes& attributes, std::vector<u32>& indices);

struct MeshletInfo
{
    std::vector<lux::assetlib::SceneAssetMeshlet> Meshlets;
    std::vector<lux::assetlib::SceneAssetIndexType> Indices;
};

MeshletInfo createMeshlets(RemapContext& ctx, Attributes& attributes, const std::vector<u32>& indices);

void remapBlendShapeAttributes(const RemapContext& ctx, Attributes& attributes);

void remapAttributes(u32 vertexCount, const std::vector<u32>& indexRemap, Attributes& remapped, Attributes& original);
void remapAttributesAsMeshlets(const std::vector<meshopt_Meshlet>& meshlets, const std::vector<u32>& meshletVertices,
    Attributes& remapped, Attributes& original);

struct BoundingVolumes
{
    Sphere Sphere;
    AABB Box;
};

BoundingVolumes meshBoundingVolumes(const std::vector<lux::assetlib::SceneAssetMeshlet>& meshlets);
}
