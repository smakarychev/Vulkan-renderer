#include "SceneUtils.h"

#include <vector>
#include <meshoptimizer.h>
#include <ranges>

namespace utils
{
// todo: is this even correct?
void remapMesh(Attributes& attributes, std::vector<u32>& indices)
{
    std::vector<meshopt_Stream> vertexElementsStreams;
    if (!attributes.Positions->empty())
        vertexElementsStreams.push_back({attributes.Positions->data(), sizeof(glm::vec3), sizeof(glm::vec3)});
    if (!attributes.Normals->empty())
        vertexElementsStreams.push_back({attributes.Normals->data(), sizeof(glm::vec3), sizeof(glm::vec3)});
    if (!attributes.Tangents->empty())
        vertexElementsStreams.push_back({attributes.Tangents->data(), sizeof(glm::vec4), sizeof(glm::vec4)});
    if (!attributes.UVs->empty())
        vertexElementsStreams.push_back({attributes.UVs->data(), sizeof(glm::vec2), sizeof(glm::vec2)});
    if (!attributes.Joints->empty())
        vertexElementsStreams.push_back({attributes.Joints->data(), sizeof(glm::u16vec4), sizeof(glm::u16vec4)});
    if (!attributes.Weights->empty())
        vertexElementsStreams.push_back({attributes.Weights->data(), sizeof(glm::vec4), sizeof(glm::vec4)});

    const u32 indexCountInitial = (u32)indices.size();
    const u32 vertexCountInitial = (u32)attributes.Positions->size();

    std::vector indexRemap(indices);
    const u32 vertexCount = (u32)meshopt_generateVertexRemapMulti(indexRemap.data(),
        indices.data(),
        indexCountInitial, vertexCountInitial,
        vertexElementsStreams.data(), vertexElementsStreams.size());

    std::vector<u32> remappedIndices(indexCountInitial);
    std::vector<glm::vec3> remappedPositions(vertexCount);
    std::vector<glm::vec3> remappedNormals(vertexCount);
    std::vector<glm::vec4> remappedTangents(vertexCount);
    std::vector<glm::vec2> remappedUVs(vertexCount);
    std::vector<glm::u16vec4> remappedJoints(vertexCount);
    std::vector<glm::vec4> remappedWeights(vertexCount);

    meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), indexRemap.data());
    if (!attributes.Positions->empty())
        meshopt_remapVertexBuffer(remappedPositions.data(), attributes.Positions->data(),
            vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
    if (!attributes.Normals->empty())
        meshopt_remapVertexBuffer(remappedNormals.data(), attributes.Normals->data(),
            vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
    if (!attributes.Tangents->empty())
        meshopt_remapVertexBuffer(remappedTangents.data(), attributes.Tangents->data(),
            vertexCountInitial, sizeof(glm::vec4), indexRemap.data());
    if (!attributes.UVs->empty())
        meshopt_remapVertexBuffer(remappedUVs.data(), attributes.UVs->data(),
            vertexCountInitial, sizeof(glm::vec2), indexRemap.data());
    if (!attributes.Joints->empty())
        meshopt_remapVertexBuffer(remappedJoints.data(), attributes.Joints->data(),
            vertexCountInitial, sizeof(glm::u16vec4), indexRemap.data());
    if (!attributes.Weights->empty())
        meshopt_remapVertexBuffer(remappedWeights.data(), attributes.Weights->data(),
            vertexCountInitial, sizeof(glm::vec4), indexRemap.data());

    meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), indexCountInitial, vertexCount);
    meshopt_optimizeVertexFetchRemap(indexRemap.data(), remappedIndices.data(), indexCountInitial, vertexCount);

    meshopt_remapIndexBuffer(remappedIndices.data(), remappedIndices.data(), indexCountInitial, indexRemap.data());
    if (!attributes.Positions->empty())
        meshopt_remapVertexBuffer(remappedPositions.data(), remappedPositions.data(),
            vertexCount, sizeof(glm::vec3), indexRemap.data());
    if (!attributes.Normals->empty())
        meshopt_remapVertexBuffer(remappedNormals.data(), remappedNormals.data(),
            vertexCount, sizeof(glm::vec3), indexRemap.data());
    if (!attributes.Tangents->empty())
        meshopt_remapVertexBuffer(remappedTangents.data(), remappedTangents.data(),
            vertexCount, sizeof(glm::vec4), indexRemap.data());
    if (!attributes.UVs->empty())
        meshopt_remapVertexBuffer(remappedUVs.data(), remappedUVs.data(),
            vertexCount, sizeof(glm::vec2), indexRemap.data());
    if (!attributes.Joints->empty())
        meshopt_remapVertexBuffer(remappedJoints.data(), remappedJoints.data(),
            vertexCount, sizeof(glm::u16vec4), indexRemap.data());
    if (!attributes.Weights->empty())
        meshopt_remapVertexBuffer(remappedWeights.data(), remappedWeights.data(),
            vertexCount, sizeof(glm::vec4), indexRemap.data());

    indices = remappedIndices;
    if (!attributes.Positions->empty())
        *attributes.Positions = remappedPositions;
    if (!attributes.Normals->empty())
        *attributes.Normals = remappedNormals;
    if (!attributes.Tangents->empty())
        *attributes.Tangents = remappedTangents;
    if (!attributes.UVs->empty())
        *attributes.UVs = remappedUVs;
    if (!attributes.Joints->empty())
        *attributes.Joints = remappedJoints;
    if (!attributes.Weights->empty())
        *attributes.Weights = remappedWeights;
}

MeshletInfo createMeshlets(Attributes& attributes, const std::vector<u32>& indices)
{
    MeshletInfo meshletInfo = {};

    f32 coneWeight = 0.5f;

    std::vector<meshopt_Meshlet> meshoptMeshlets(meshopt_buildMeshletsBound(indices.size(),
        lux::assetlib::SceneAsset::VERTICES_PER_MESHLET, lux::assetlib::SceneAsset::TRIANGLES_PER_MESHLET));
    std::vector<u32> meshletVertices(meshoptMeshlets.size() * lux::assetlib::SceneAsset::VERTICES_PER_MESHLET);
    meshletInfo.Indices.resize(meshoptMeshlets.size() * lux::assetlib::SceneAsset::TRIANGLES_PER_MESHLET * 3);

    meshoptMeshlets.resize(meshopt_buildMeshlets(meshoptMeshlets.data(),
        meshletVertices.data(), meshletInfo.Indices.data(),
        indices.data(), indices.size(),
        (f32*)attributes.Positions->data(), attributes.Positions->size(), sizeof(glm::vec3),
        lux::assetlib::SceneAsset::VERTICES_PER_MESHLET, lux::assetlib::SceneAsset::TRIANGLES_PER_MESHLET, coneWeight));

    const meshopt_Meshlet& lastMeshlet = meshoptMeshlets.back();

    meshletVertices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
    meshletInfo.Indices.resize(lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3));

    meshletInfo.Meshlets.reserve(meshoptMeshlets.size());
    for (const auto& meshoptMeshlet : meshoptMeshlets)
    {
        lux::assetlib::SceneAssetMeshlet meshlet = {
            .FirstIndex = meshoptMeshlet.triangle_offset,
            .IndexCount = meshoptMeshlet.triangle_count * 3,
            .FirstVertex = meshoptMeshlet.vertex_offset,
            .VertexCount = meshoptMeshlet.vertex_count
        };

        meshopt_Bounds meshoptBounds = meshopt_computeMeshletBounds(&meshletVertices[meshoptMeshlet.vertex_offset],
            &meshletInfo.Indices[meshoptMeshlet.triangle_offset], meshoptMeshlet.triangle_count,
            (f32*)attributes.Positions->data(), attributes.Positions->size(), sizeof(glm::vec3));

        meshlet.Sphere = Sphere{
            .Center = glm::vec3{meshoptBounds.center[0], meshoptBounds.center[1], meshoptBounds.center[2]},
            .Radius = meshoptBounds.radius
        };

        meshlet.Cone = lux::assetlib::SceneAssetMeshlet::BoundingCone{
            .AxisX = meshoptBounds.cone_axis_s8[0],
            .AxisY = meshoptBounds.cone_axis_s8[1],
            .AxisZ = meshoptBounds.cone_axis_s8[2],
            .Cutoff = meshoptBounds.cone_cutoff_s8
        };

        meshletInfo.Meshlets.push_back(meshlet);
    }

    std::vector<glm::vec3> finalPositions(meshletVertices.size());
    std::vector<glm::vec3> finalNormals(meshletVertices.size());
    std::vector<glm::vec4> finalTangents(meshletVertices.size());
    std::vector<glm::vec2> finalUVs(meshletVertices.size());
    std::vector<glm::u16vec4> finalJoints(meshletVertices.size());
    std::vector<glm::vec4> finalWeights(meshletVertices.size());

    for (auto& meshlet : meshoptMeshlets)
    {
        u32 vertexOffset = meshlet.vertex_offset;
        for (u32 localIndex = 0; localIndex < meshlet.vertex_count; localIndex++)
        {
            u32 vertexIndex = vertexOffset + localIndex;
            if (!attributes.Positions->empty())
                finalPositions[vertexIndex] = (*attributes.Positions)[meshletVertices[vertexIndex]];
            if (!attributes.Normals->empty())
                finalNormals[vertexIndex] = (*attributes.Normals)[meshletVertices[vertexIndex]];
            if (!attributes.Tangents->empty())
                finalTangents[vertexIndex] = (*attributes.Tangents)[meshletVertices[vertexIndex]];
            if (!attributes.UVs->empty())
                finalUVs[vertexIndex] = (*attributes.UVs)[meshletVertices[vertexIndex]];
            if (!attributes.Joints->empty())
                finalJoints[vertexIndex] = (*attributes.Joints)[meshletVertices[vertexIndex]];
            if (!attributes.Weights->empty())
                finalWeights[vertexIndex] = (*attributes.Weights)[meshletVertices[vertexIndex]];
        }
    }

    if (!attributes.Positions->empty())
        *attributes.Positions = finalPositions;
    if (!attributes.Normals->empty())
        *attributes.Normals = finalNormals;
    if (!attributes.Tangents->empty())
        *attributes.Tangents = finalTangents;
    if (!attributes.UVs->empty())
        *attributes.UVs = finalUVs;
    if (!attributes.Joints->empty())
        *attributes.Joints = finalJoints;
    if (!attributes.Weights->empty())
        *attributes.Weights = finalWeights;

    return meshletInfo;
}

BoundingVolumes meshBoundingVolumes(const std::vector<lux::assetlib::SceneAssetMeshlet>& meshlets)
{
    if (meshlets.empty())
        return {
            {.Center = glm::vec3{0.0f}, .Radius = 0.0f},
            {.Min = glm::vec3{0.0f}, .Max = glm::vec3{0.0f}}
        };

    Sphere boundingSphere = meshlets.front().Sphere;
    AABB boundingBox = AABB::FromSphere(boundingSphere);
    for (auto& meshlet : meshlets | std::views::drop(1))
    {
        boundingSphere = boundingSphere.Merge(meshlet.Sphere);
        boundingBox = boundingBox.Merge(AABB::FromSphere(meshlet.Sphere));
    }

    return {
        .Sphere = boundingSphere,
        .Box = boundingBox
    };
}
}
