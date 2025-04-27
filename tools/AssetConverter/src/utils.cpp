#include "utils.h"

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <meshoptimizer.h>
#include <ranges>

namespace Utils
{
    void remapMesh(Attributes& attributes, std::vector<u32>& indices)
    {
        std::array<meshopt_Stream, (u32)assetLib::VertexElement::MaxVal> vertexElementsStreams = {{
            {attributes.Positions->data(), sizeof(glm::vec3), sizeof(glm::vec3)},
            {attributes.Normals->data(), sizeof(glm::vec3), sizeof(glm::vec3)},
            {attributes.Tangents->data(), sizeof(glm::vec4), sizeof(glm::vec4)},
            {attributes.UVs->data(), sizeof(glm::vec2), sizeof(glm::vec2)},
        }};

        u32 indexCountInitial = (u32)indices.size();
        u32 vertexCountInitial = (u32)attributes.Positions->size();
        
        std::vector<u32> indexRemap(indices);
        u32 vertexCount = (u32)meshopt_generateVertexRemapMulti(indexRemap.data(),
            indices.data(),
            indexCountInitial, vertexCountInitial,
            vertexElementsStreams.data(), vertexElementsStreams.size());

        std::vector<u32> remappedIndices(indexCountInitial);
        std::vector<glm::vec3> remappedPositions(vertexCount);
        std::vector<glm::vec3> remappedNormals(vertexCount);
        std::vector<glm::vec4> remappedTangents(vertexCount);
        std::vector<glm::vec2> remappedUVs(vertexCount);

        meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), indexRemap.data());
        meshopt_remapVertexBuffer(remappedPositions.data(), attributes.Positions->data(),
            vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedNormals.data(), attributes.Normals->data(),
            vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedTangents.data(), attributes.Tangents->data(),
            vertexCountInitial, sizeof(glm::vec4), indexRemap.data());
        meshopt_remapVertexBuffer(remappedUVs.data(), attributes.UVs->data(),
            vertexCountInitial, sizeof(glm::vec2), indexRemap.data());

        meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), indexCountInitial, vertexCount);
        meshopt_optimizeVertexFetchRemap(indexRemap.data(), remappedIndices.data(), indexCountInitial, vertexCount);

        meshopt_remapIndexBuffer(remappedIndices.data(), remappedIndices.data(), indexCountInitial, indexRemap.data());
        meshopt_remapVertexBuffer(remappedPositions.data(), remappedPositions.data(),
            vertexCount, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedNormals.data(), remappedNormals.data(),
            vertexCount, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedTangents.data(), remappedTangents.data(),
            vertexCount, sizeof(glm::vec4), indexRemap.data());
        meshopt_remapVertexBuffer(remappedUVs.data(), remappedUVs.data(),
            vertexCount, sizeof(glm::vec2), indexRemap.data());

        indices = remappedIndices;
        *attributes.Positions = remappedPositions;
        *attributes.Normals = remappedNormals;
        *attributes.Tangents = remappedTangents;
        *attributes.UVs = remappedUVs;
    }

    MeshletInfo createMeshlets(Attributes& attributes, const std::vector<u32>& indices)
    {
        MeshletInfo meshletInfo = {};
        
        f32 coneWeight = 0.5f;

        std::vector<meshopt_Meshlet> meshoptMeshlets(meshopt_buildMeshletsBound(indices.size(),
            assetLib::SceneInfo::VERTICES_PER_MESHLET, assetLib::SceneInfo::TRIANGLES_PER_MESHLET));
        std::vector<u32> meshletVertices(meshoptMeshlets.size() * assetLib::SceneInfo::VERTICES_PER_MESHLET);
        meshletInfo.Indices.resize(meshoptMeshlets.size() * assetLib::SceneInfo::TRIANGLES_PER_MESHLET * 3);

        meshoptMeshlets.resize(meshopt_buildMeshlets(meshoptMeshlets.data(),
            meshletVertices.data(), meshletInfo.Indices.data(),
            indices.data(), indices.size(),
            (f32*)attributes.Positions->data(), attributes.Positions->size(), sizeof(glm::vec3),
            assetLib::SceneInfo::VERTICES_PER_MESHLET, assetLib::SceneInfo::TRIANGLES_PER_MESHLET, coneWeight));

        const meshopt_Meshlet& lastMeshlet = meshoptMeshlets.back();

        meshletVertices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
        meshletInfo.Indices.resize(lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3));

        meshletInfo.Meshlets.reserve(meshoptMeshlets.size());
        for (const auto& meshoptMeshlet : meshoptMeshlets)
        {
            assetLib::SceneInfo::Meshlet meshlet = {
                .FirstIndex = meshoptMeshlet.triangle_offset,
                .IndexCount = meshoptMeshlet.triangle_count * 3,
                .FirstVertex = meshoptMeshlet.vertex_offset,
                .VertexCount = meshoptMeshlet.vertex_count};

            meshopt_Bounds meshoptBounds = meshopt_computeMeshletBounds(&meshletVertices[meshoptMeshlet.vertex_offset],
                &meshletInfo.Indices[meshoptMeshlet.triangle_offset], meshoptMeshlet.triangle_count,
                (f32*)attributes.Positions->data(), attributes.Positions->size(), sizeof(glm::vec3));

            meshlet.BoundingSphere = Sphere{
                .Center = glm::vec3{meshoptBounds.center[0], meshoptBounds.center[1], meshoptBounds.center[2]},
                .Radius = meshoptBounds.radius};

            meshlet.BoundingCone = assetLib::SceneInfo::BoundingCone{
                .AxisX = meshoptBounds.cone_axis_s8[0],
                .AxisY = meshoptBounds.cone_axis_s8[1],
                .AxisZ = meshoptBounds.cone_axis_s8[2],
                .Cutoff = meshoptBounds.cone_cutoff_s8};

            meshletInfo.Meshlets.push_back(meshlet);
        }

        std::vector<glm::vec3> finalPositions(meshletVertices.size());
        std::vector<glm::vec3> finalNormals(meshletVertices.size());
        std::vector<glm::vec4> finalTangents(meshletVertices.size());
        std::vector<glm::vec2> finalUVs(meshletVertices.size());
        
        for (auto& meshlet : meshoptMeshlets)
        {
            u32 vertexOffset = meshlet.vertex_offset;
            for (u32 localIndex = 0; localIndex < meshlet.vertex_count; localIndex++)
            {
                u32 vertexIndex = vertexOffset + localIndex;
                finalPositions[vertexIndex] = (*attributes.Positions)[meshletVertices[vertexIndex]];
                finalNormals[vertexIndex] = (*attributes.Normals)[meshletVertices[vertexIndex]];
                finalTangents[vertexIndex] = (*attributes.Tangents)[meshletVertices[vertexIndex]];
                finalUVs[vertexIndex] = (*attributes.UVs)[meshletVertices[vertexIndex]];
            }
        }
        
        *attributes.Positions = finalPositions;
        *attributes.Normals = finalNormals;
        *attributes.Tangents = finalTangents;
        *attributes.UVs = finalUVs;

        return meshletInfo;
    }

    BoundingVolumes meshBoundingVolumes(const std::vector<assetLib::SceneInfo::Meshlet>& meshlets)
    {
        if (meshlets.empty())
            return {
                {.Center = glm::vec3{0.0f}, .Radius = 0.0f},
                {.Min = glm::vec3{0.0f}, .Max = glm::vec3{0.0f}}};

        Sphere boundingSphere = meshlets.front().BoundingSphere;
        AABB boundingBox = AABB::FromSphere(boundingSphere);
        for (auto& meshlet : meshlets | std::ranges::views::drop(1))
        {
            boundingSphere = boundingSphere.Merge(meshlet.BoundingSphere);
            boundingBox = boundingBox.Merge(AABB::FromSphere(meshlet.BoundingSphere));
        }

        return {
            .Sphere = boundingSphere,
            .Box = boundingBox};
    }
}
