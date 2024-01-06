#include "utils.h"

#include <algorithm>
#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <meshoptimizer.h>

#include "Core/core.h"


namespace
{
    assetLib::BoundingSphere sphereBy2Points(const std::vector<glm::vec3>& boundaryPoints)
    {
        glm::vec3 center = (boundaryPoints[0] + boundaryPoints[1]) * 0.5f;

        f32 radius = glm::length(boundaryPoints[0] - boundaryPoints[1]) * 0.5f;

        return {.Center = center, .Radius = radius};
    }
    
    assetLib::BoundingSphere sphereBy3Points(const std::vector<glm::vec3>& boundaryPoints)
    {
        glm::vec3 a = boundaryPoints[0] - boundaryPoints[2];
        glm::vec3 b = boundaryPoints[1] - boundaryPoints[2];

        glm::vec3 cross = glm::cross(a, b);

        glm::vec3 center = boundaryPoints[2] + glm::cross(glm::length2(a) * b - glm::length2(b) * a, cross)
            / (2.0f * glm::length2(cross));

        f32 radius = glm::distance(boundaryPoints[0], center);

        return {.Center = center, .Radius = radius};
    }

    assetLib::BoundingSphere sphereBy4Points(const std::vector<glm::vec3>& boundaryPoints)
    {
        glm::vec3 a = boundaryPoints[1] - boundaryPoints[0];
        glm::vec3 b = boundaryPoints[2] - boundaryPoints[0];
        glm::vec3 c = boundaryPoints[3] - boundaryPoints[0];

        glm::vec3 squares = {
            glm::length2(a),
            glm::length2(b),
            glm::length2(c),
        };
        
        glm::vec3 center = boundaryPoints[0] +
            (squares[0] * glm::cross(b, c) + squares[1] * glm::cross(c, a) + squares[2] * glm::cross(a, b)) /
            (2.0f * glm::dot(a, glm::cross(b, c)));

        f32 radius = glm::distance(center, boundaryPoints[0]);
        
        return {.Center = center, .Radius = radius};
    }
    
    assetLib::BoundingSphere sphereByPoints(const std::vector<glm::vec3>& boundaryPoints)
    {
        if (boundaryPoints.empty())
            return {.Center = glm::vec3(0.0f), .Radius = 0.0f};
        if (boundaryPoints.size() == 1)
            return {.Center = boundaryPoints.front(), .Radius = 0.0f};
        if (boundaryPoints.size() == 2)
            return sphereBy2Points(boundaryPoints);
        if (boundaryPoints.size() == 3)
            return sphereBy3Points(boundaryPoints);
       return sphereBy4Points(boundaryPoints);
    }

    bool isInSphere(const glm::vec3& point, const assetLib::BoundingSphere& sphere)
    {
        return glm::distance(point, sphere.Center) - sphere.Radius < 1e-7f;
    }

}

namespace utils
{
    assetLib::BoundingSphere welzlSphere(std::vector<glm::vec3> points)
    {
        static std::random_device device;
        static std::mt19937 generator{device()};
        static std::uniform_real_distribution<f32> f32Distribution(0, 1);
        
        std::ranges::shuffle(points, generator);

        std::vector<assetLib::BoundingSphere> results;
        
        enum class RecursionBranch {First, Second};
        struct Frame
        {
            u32 PointCount;
            std::optional<glm::vec3> TestPoint;
            std::vector<glm::vec3> BoundaryPoints;
            RecursionBranch Branch;

            Frame(u32 pointCount, const std::vector<glm::vec3>& boundary, RecursionBranch branch)
                : PointCount(pointCount), BoundaryPoints(boundary), Branch(branch) {}
            Frame(u32 pointCount, const std::vector<glm::vec3>& boundary, const glm::vec3& testPoint, RecursionBranch branch)
                : PointCount(pointCount), TestPoint(testPoint), BoundaryPoints(boundary), Branch(branch) {}
        };

        std::vector<Frame> frameStack;
        frameStack.emplace_back((u32)points.size(), std::vector<glm::vec3>{}, RecursionBranch::First);

        while (!frameStack.empty())
        {
            auto [count, testPoint, boundary, branch] = frameStack.back(); frameStack.pop_back();
            if (branch == RecursionBranch::First)
            {
                if (count == 0 || boundary.size() == 4)
                {
                    results.push_back(sphereByPoints(boundary));
                }
                else
                {
                    u32 random = u32(f32Distribution(generator) * (f32)count);
                    glm::vec3 point = points[random];
                    std::swap(points[random], points[count - 1]);

                    frameStack.emplace_back(count, boundary, point, RecursionBranch::Second);
                    frameStack.emplace_back(count - 1, boundary, RecursionBranch::First);
                }
            }
            else
            {
                assetLib::BoundingSphere testSphere = results.back(); results.pop_back();
                if (isInSphere(*testPoint, testSphere))
                {
                    results.push_back(testSphere);
                }
                else
                {
                    boundary.push_back(*testPoint);
                    frameStack.emplace_back(count - 1, boundary, RecursionBranch::First);
                }
            }
        }
        
        return results.back();
    }

    u32 nextPowerOf2(u32 number)
    {
        ASSERT(number != 0, "Number must be positive")
        number--;

        number |= number >> 1;
        number |= number >> 2;
        number |= number >> 4;
        number |= number >> 8;
        number |= number >> 16;

        return number + 1;
    }

    void remapMesh(ModelConverter::MeshData& meshData, std::vector<u32>& indices)
    {
        static constexpr u32 NON_INDEX = std::numeric_limits<u32>::max();

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
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Positions.data(), meshData.VertexGroup.Positions.data(), vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Normals.data(), meshData.VertexGroup.Normals.data(), vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Tangents.data(), meshData.VertexGroup.Tangents.data(), vertexCountInitial, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.UVs.data(), meshData.VertexGroup.UVs.data(), vertexCountInitial, sizeof(glm::vec2), indexRemap.data());

        meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), indexCountInitial, vertexCount);
        meshopt_optimizeVertexFetchRemap(indexRemap.data(), remappedIndices.data(), indexCountInitial, vertexCount);

        meshopt_remapIndexBuffer(remappedIndices.data(), remappedIndices.data(), indexCountInitial, indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Positions.data(), remappedMesh.VertexGroup.Positions.data(), vertexCount, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Normals.data(), remappedMesh.VertexGroup.Normals.data(), vertexCount, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.Tangents.data(), remappedMesh.VertexGroup.Tangents.data(), vertexCount, sizeof(glm::vec3), indexRemap.data());
        meshopt_remapVertexBuffer(remappedMesh.VertexGroup.UVs.data(), remappedMesh.VertexGroup.UVs.data(), vertexCount, sizeof(glm::vec2), indexRemap.data());

        indices.clear();
        indices.reserve(remappedIndices.size());
        for (auto index : remappedIndices)
            indices.push_back(index);
        meshData.VertexGroup.Positions = remappedMesh.VertexGroup.Positions;
        meshData.VertexGroup.Normals = remappedMesh.VertexGroup.Normals;
        meshData.VertexGroup.Tangents = remappedMesh.VertexGroup.Tangents;
        meshData.VertexGroup.UVs = remappedMesh.VertexGroup.UVs;
    }

    std::vector<assetLib::ModelInfo::Meshlet> createMeshlets(ModelConverter::MeshData& meshData, std::vector<u32>& indices)
    {
        f32 coneWeight = 0.5f;

        std::vector<meshopt_Meshlet> meshoptMeshlets(meshopt_buildMeshletsBound(indices.size(),
            assetLib::ModelInfo::VERTICES_PER_MESHLET, assetLib::ModelInfo::TRIANGLES_PER_MESHLET));
        std::vector<u32> meshletVertices(meshoptMeshlets.size() * assetLib::ModelInfo::VERTICES_PER_MESHLET);
        std::vector<u8> meshletTriangles(meshoptMeshlets.size() * assetLib::ModelInfo::TRIANGLES_PER_MESHLET * 3);

        meshoptMeshlets.resize(meshopt_buildMeshlets(meshoptMeshlets.data(), meshletVertices.data(), meshletTriangles.data(),
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
                finalMeshData.VertexGroup.Positions[vertexIndex] = meshData.VertexGroup.Positions[meshletVertices[vertexIndex]];
                finalMeshData.VertexGroup.Normals[vertexIndex] = meshData.VertexGroup.Normals[meshletVertices[vertexIndex]];
                finalMeshData.VertexGroup.Tangents[vertexIndex] = meshData.VertexGroup.Tangents[meshletVertices[vertexIndex]];
                finalMeshData.VertexGroup.UVs[vertexIndex] = meshData.VertexGroup.UVs[meshletVertices[vertexIndex]];
            }
        }
        
        meshData.Indices = finalMeshData.Indices;
        meshData.VertexGroup = finalMeshData.VertexGroup;

        return meshlets;
    }
}
