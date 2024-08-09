#include "utils.h"

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <meshoptimizer.h>
#include <ranges>

namespace
{
    assetLib::BoundingSphere mergeSpheres(const assetLib::BoundingSphere& a, const assetLib::BoundingSphere& b)
    {
        f32 distance = glm::length(a.Center - b.Center);

        if (distance <= std::abs(a.Radius - b.Radius)) 
            return a.Radius > b.Radius ? a : b;

        f32 newRadius = (distance + a.Radius + b.Radius) / 2.0f;
        glm::vec3 newCenter = a.Center + (b.Center - a.Center) * (newRadius - a.Radius) / distance;

        return {newCenter, newRadius};
    }

    assetLib::BoundingBox boxFromSphere(const assetLib::BoundingSphere& sphere)
    {
        return {
            .Min = sphere.Center - sphere.Radius,
            .Max = sphere.Center + sphere.Radius};
    }

    assetLib::BoundingBox mergeBoxes(const assetLib::BoundingBox& a, const assetLib::BoundingBox& b)
    {
        return {
            .Min = glm::min(a.Min, b.Min),
            .Max = glm::max(a.Max, b.Max)};
    }
}

namespace Utils
{
    void remapMesh(ModelConverter::MeshData& meshData, std::vector<u32>& indices)
    {
        std::array<meshopt_Stream, (u32)assetLib::VertexElement::MaxVal> vertexElementsStreams = {{
            {meshData.VertexGroup.Positions.data(), sizeof(glm::vec3), sizeof(glm::vec3)},
            {meshData.VertexGroup.Normals.data(), sizeof(glm::vec3), sizeof(glm::vec3)},
            {meshData.VertexGroup.Tangents.data(), sizeof(glm::vec3), sizeof(glm::vec3)},
            {meshData.VertexGroup.UVs.data(), sizeof(glm::vec2), sizeof(glm::vec2)},
        }};

        u32 indexCountInitial = (u32)indices.size();
        u32 vertexCountInitial = (u32)meshData.VertexGroup.Positions.size();
        
        std::vector<u32> indexRemap(indices);
        u32 vertexCount = (u32)meshopt_generateVertexRemapMulti(indexRemap.data(),
            indices.data(),
            indexCountInitial, vertexCountInitial,
            vertexElementsStreams.data(), vertexElementsStreams.size());

        ModelConverter::MeshData remappedMesh;
        std::vector<u32> remappedIndices;
        remappedIndices.resize(indexCountInitial);
        remappedMesh.VertexGroup.Positions.resize(vertexCount);
        remappedMesh.VertexGroup.Normals.resize(vertexCount);
        remappedMesh.VertexGroup.Tangents.resize(vertexCount);
        remappedMesh.VertexGroup.UVs.resize(vertexCount);

        meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Positions.data(), meshData.VertexGroup.Positions.data(),
            vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Normals.data(), meshData.VertexGroup.Normals.data(),
            vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Tangents.data(), meshData.VertexGroup.Tangents.data(),
            vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.UVs.data(), meshData.VertexGroup.UVs.data(),
            vertexCountInitial, sizeof(glm::vec2), indexRemap.data());

        meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), indexCountInitial, vertexCount);
        meshopt_optimizeVertexFetchRemap(indexRemap.data(), remappedIndices.data(), indexCountInitial, vertexCount);

        meshopt_remapIndexBuffer(remappedIndices.data(), remappedIndices.data(), indexCountInitial, indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Positions.data(), remappedMesh.VertexGroup.Positions.data(),
            vertexCount, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Normals.data(), remappedMesh.VertexGroup.Normals.data(),
            vertexCount, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Tangents.data(), remappedMesh.VertexGroup.Tangents.data(),
            vertexCount, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.UVs.data(), remappedMesh.VertexGroup.UVs.data(),
            vertexCount, sizeof(glm::vec2), indexRemap.data());

        indices.clear();
        indices.reserve(remappedIndices.size());
        for (auto index : remappedIndices)
            indices.push_back(index);
        meshData.VertexGroup.Positions = remappedMesh.VertexGroup.Positions;
        meshData.VertexGroup.Normals = remappedMesh.VertexGroup.Normals;
        meshData.VertexGroup.Tangents = remappedMesh.VertexGroup.Tangents;
        meshData.VertexGroup.UVs = remappedMesh.VertexGroup.UVs;
    }

    std::vector<assetLib::ModelInfo::Meshlet> createMeshlets(ModelConverter::MeshData& meshData,
        const std::vector<u32>& indices)
    {
        f32 coneWeight = 0.5f;

        std::vector<meshopt_Meshlet> meshoptMeshlets(meshopt_buildMeshletsBound(indices.size(),
            assetLib::ModelInfo::VERTICES_PER_MESHLET, assetLib::ModelInfo::TRIANGLES_PER_MESHLET));
        std::vector<u32> meshletVertices(meshoptMeshlets.size() * assetLib::ModelInfo::VERTICES_PER_MESHLET);
        std::vector<u8> meshletTriangles(meshoptMeshlets.size() * assetLib::ModelInfo::TRIANGLES_PER_MESHLET * 3);

        meshoptMeshlets.resize(meshopt_buildMeshlets(meshoptMeshlets.data(),
            meshletVertices.data(), meshletTriangles.data(),
            indices.data(), indices.size(),
            (f32*)meshData.VertexGroup.Positions.data(), meshData.VertexGroup.Positions.size(), sizeof(glm::vec3),
            assetLib::ModelInfo::VERTICES_PER_MESHLET, assetLib::ModelInfo::TRIANGLES_PER_MESHLET, coneWeight));

        const meshopt_Meshlet& lastMeshlet = meshoptMeshlets.back();

        meshletVertices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
        meshletTriangles.resize(lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3));

        std::vector<assetLib::ModelInfo::Meshlet> meshlets;
        
        for (const auto& meshoptMeshlet : meshoptMeshlets)
        {
            assetLib::ModelInfo::Meshlet meshlet = {
                .FirstIndex = meshoptMeshlet.triangle_offset,
                .IndexCount = meshoptMeshlet.triangle_count * 3,
                .FirstVertex = meshoptMeshlet.vertex_offset,
                .VertexCount = meshoptMeshlet.vertex_count};

            meshopt_Bounds meshoptBounds = meshopt_computeMeshletBounds(&meshletVertices[meshoptMeshlet.vertex_offset],
                &meshletTriangles[meshoptMeshlet.triangle_offset], meshoptMeshlet.triangle_count,
                (f32*)meshData.VertexGroup.Positions.data(), meshData.VertexGroup.Positions.size(), sizeof(glm::vec3));

            meshlet.BoundingSphere = assetLib::BoundingSphere{
                .Center = glm::vec3{meshoptBounds.center[0], meshoptBounds.center[1], meshoptBounds.center[2]},
                .Radius = meshoptBounds.radius};

            meshlet.BoundingCone = assetLib::BoundingCone{
                .AxisX = meshoptBounds.cone_axis_s8[0],
                .AxisY = meshoptBounds.cone_axis_s8[1],
                .AxisZ = meshoptBounds.cone_axis_s8[2],
                .Cutoff = meshoptBounds.cone_cutoff_s8};

            meshlets.push_back(meshlet);
        }

        ModelConverter::MeshData finalMeshData = meshData;
        finalMeshData.Indices.clear();
        finalMeshData.Indices.reserve(meshletTriangles.size());
        for (auto index : meshletTriangles)
            finalMeshData.Indices.push_back(index);

        finalMeshData.VertexGroup.Positions.resize(meshletVertices.size());
        finalMeshData.VertexGroup.Normals.resize(meshletVertices.size());
        finalMeshData.VertexGroup.Tangents.resize(meshletVertices.size());
        finalMeshData.VertexGroup.UVs.resize(meshletVertices.size());

        for (auto& meshlet : meshoptMeshlets)
        {
            u32 vertexOffset = meshlet.vertex_offset;
            for (u32 localIndex = 0; localIndex < meshlet.vertex_count; localIndex++)
            {
                u32 vertexIndex = vertexOffset + localIndex;
                finalMeshData.VertexGroup.Positions[vertexIndex] =
                    meshData.VertexGroup.Positions[meshletVertices[vertexIndex]];
                finalMeshData.VertexGroup.Normals[vertexIndex] =
                    meshData.VertexGroup.Normals[meshletVertices[vertexIndex]];
                finalMeshData.VertexGroup.Tangents[vertexIndex] =
                    meshData.VertexGroup.Tangents[meshletVertices[vertexIndex]];
                finalMeshData.VertexGroup.UVs[vertexIndex] =
                    meshData.VertexGroup.UVs[meshletVertices[vertexIndex]];
            }
        }
        
        meshData.Indices = finalMeshData.Indices;
        meshData.VertexGroup = finalMeshData.VertexGroup;

        return meshlets;
    }

    BoundingVolumes meshBoundingVolumes(const std::vector<assetLib::ModelInfo::Meshlet>& meshlets)
    {
        if (meshlets.empty())
            return {
                {.Center = glm::vec3{0.0f}, .Radius = 0.0f},
                {.Min = glm::vec3{0.0f}, .Max = glm::vec3{0.0f}}};

        assetLib::BoundingSphere boundingSphere = meshlets.front().BoundingSphere;
        assetLib::BoundingBox boundingBox = boxFromSphere(boundingSphere);
        for (auto& meshlet : meshlets | std::ranges::views::drop(1))
        {
            boundingSphere = mergeSpheres(boundingSphere, meshlet.BoundingSphere);
            boundingBox = mergeBoxes(boundingBox, boxFromSphere(meshlet.BoundingSphere));
        }

        return {
            .Sphere = boundingSphere,
            .Box = boundingBox};
    }
}
