#include "SceneUtils.h"

#include <vector>
#include <meshoptimizer.h>
#include <ranges>

namespace utils
{
void Attributes::Init(u32 vertexCount)
{
    Positions.resize(vertexCount);
    Normals.resize(vertexCount);
    Tangents.resize(vertexCount);
    UVs.resize(vertexCount);
    Joints.resize(vertexCount);
    Weights.resize(vertexCount);
}

void remapMesh(RemapContext& ctx, Attributes& attributes, std::vector<u32>& indices)
{
    std::vector<meshopt_Stream> vertexElementsStreams;
    if (!attributes.Positions.empty())
        vertexElementsStreams.push_back({attributes.Positions.data(), sizeof(glm::vec3), sizeof(glm::vec3)});
    if (!attributes.Normals.empty())
        vertexElementsStreams.push_back({attributes.Normals.data(), sizeof(glm::vec3), sizeof(glm::vec3)});
    if (!attributes.Tangents.empty())
        vertexElementsStreams.push_back({attributes.Tangents.data(), sizeof(glm::vec4), sizeof(glm::vec4)});
    if (!attributes.UVs.empty())
        vertexElementsStreams.push_back({attributes.UVs.data(), sizeof(glm::vec2), sizeof(glm::vec2)});
    if (!attributes.Joints.empty())
        vertexElementsStreams.push_back({attributes.Joints.data(), sizeof(glm::u16vec4), sizeof(glm::u16vec4)});
    if (!attributes.Weights.empty())
        vertexElementsStreams.push_back({attributes.Weights.data(), sizeof(glm::vec4), sizeof(glm::vec4)});

    const u32 indexCountInitial = (u32)indices.size();
    ctx.InitialVertexCount = (u32)attributes.Positions.size();

    ctx.FinalIndexRemap = indices;
    ctx.RemappedVertexCount = (u32)meshopt_generateVertexRemapMulti(ctx.FinalIndexRemap.data(),
        indices.data(),
        indexCountInitial, ctx.InitialVertexCount,
        vertexElementsStreams.data(), vertexElementsStreams.size());
    ctx.InitialIndexRemap = ctx.FinalIndexRemap;

    std::vector<u32> remappedIndices(indexCountInitial);
    Attributes remappedAttributes = {};
    remappedAttributes.Init(ctx.RemappedVertexCount);

    meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), ctx.InitialIndexRemap.data());
    remapAttributes(ctx.InitialVertexCount, ctx.InitialIndexRemap, remappedAttributes, attributes);

    meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), indexCountInitial, ctx.RemappedVertexCount);
    meshopt_optimizeVertexFetchRemap(ctx.FinalIndexRemap.data(), remappedIndices.data(), indexCountInitial,
        ctx.RemappedVertexCount);

    meshopt_remapIndexBuffer(remappedIndices.data(), remappedIndices.data(), indexCountInitial, 
        ctx.FinalIndexRemap.data());
    remapAttributes(ctx.RemappedVertexCount, ctx.FinalIndexRemap, remappedAttributes, remappedAttributes);

    indices = remappedIndices;
    attributes = std::move(remappedAttributes);
}

MeshletInfo createMeshlets(RemapContext& ctx, Attributes& attributes, const std::vector<u32>& indices)
{
    MeshletInfo meshletInfo = {};

    f32 coneWeight = 0.5f;

    ctx.MeshoptMeshlets.resize(meshopt_buildMeshletsBound(indices.size(),
        lux::assetlib::SceneAsset::VERTICES_PER_MESHLET, lux::assetlib::SceneAsset::TRIANGLES_PER_MESHLET));
    ctx.MeshoptMeshletsVertices.resize(ctx.MeshoptMeshlets.size() * lux::assetlib::SceneAsset::VERTICES_PER_MESHLET);
    meshletInfo.Indices.resize(ctx.MeshoptMeshlets.size() * lux::assetlib::SceneAsset::TRIANGLES_PER_MESHLET * 3);

    ctx.MeshoptMeshlets.resize(meshopt_buildMeshlets(ctx.MeshoptMeshlets.data(),
        ctx.MeshoptMeshletsVertices.data(), meshletInfo.Indices.data(),
        indices.data(), indices.size(),
        (f32*)attributes.Positions.data(), attributes.Positions.size(), sizeof(glm::vec3),
        lux::assetlib::SceneAsset::VERTICES_PER_MESHLET, lux::assetlib::SceneAsset::TRIANGLES_PER_MESHLET, coneWeight));

    const meshopt_Meshlet& lastMeshlet = ctx.MeshoptMeshlets.back();

    ctx.MeshoptMeshletsVertices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
    meshletInfo.Indices.resize(lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3));

    meshletInfo.Meshlets.reserve(ctx.MeshoptMeshlets.size());
    for (const auto& meshoptMeshlet : ctx.MeshoptMeshlets)
    {
        lux::assetlib::SceneAssetMeshlet meshlet = {
            .FirstIndex = meshoptMeshlet.triangle_offset,
            .IndexCount = meshoptMeshlet.triangle_count * 3,
            .FirstVertex = meshoptMeshlet.vertex_offset,
            .VertexCount = meshoptMeshlet.vertex_count
        };

        meshopt_Bounds meshoptBounds = meshopt_computeMeshletBounds(
            &ctx.MeshoptMeshletsVertices[meshoptMeshlet.vertex_offset],
            &meshletInfo.Indices[meshoptMeshlet.triangle_offset], meshoptMeshlet.triangle_count,
            (f32*)attributes.Positions.data(), attributes.Positions.size(), sizeof(glm::vec3));

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

    Attributes remapped = {};
    remapped.Init((u32)ctx.MeshoptMeshletsVertices.size());

    remapAttributesAsMeshlets(ctx.MeshoptMeshlets, ctx.MeshoptMeshletsVertices, remapped, attributes);
    attributes = std::move(remapped);

    return meshletInfo;
}

void remapBlendShapeAttributes(const RemapContext& ctx, Attributes& attributes)
{
    Attributes remappedAttributes = {};
    remappedAttributes.Init(ctx.RemappedVertexCount);
    remapAttributes(ctx.InitialVertexCount, ctx.InitialIndexRemap, remappedAttributes, attributes);
    remapAttributes(ctx.RemappedVertexCount, ctx.FinalIndexRemap, remappedAttributes, remappedAttributes);
    
    Attributes finalAttributes = {};
    finalAttributes.Init((u32)ctx.MeshoptMeshletsVertices.size());
    remapAttributesAsMeshlets(ctx.MeshoptMeshlets, ctx.MeshoptMeshletsVertices, finalAttributes, remappedAttributes);
    
    attributes = std::move(finalAttributes);
}

void remapAttributes(u32 vertexCount, const std::vector<u32>& indexRemap, Attributes& remapped, Attributes& original)
{
    if (!original.Positions.empty())
        meshopt_remapVertexBuffer(remapped.Positions.data(), original.Positions.data(),
            vertexCount, sizeof(glm::vec3), indexRemap.data());
    if (!original.Normals.empty())
        meshopt_remapVertexBuffer(remapped.Normals.data(), original.Normals.data(),
            vertexCount, sizeof(glm::vec3), indexRemap.data());
    if (!original.Tangents.empty())
        meshopt_remapVertexBuffer(remapped.Tangents.data(), original.Tangents.data(),
            vertexCount, sizeof(glm::vec4), indexRemap.data());
    if (!original.UVs.empty())
        meshopt_remapVertexBuffer(remapped.UVs.data(), original.UVs.data(),
            vertexCount, sizeof(glm::vec2), indexRemap.data());
    if (!original.Joints.empty())
        meshopt_remapVertexBuffer(remapped.Joints.data(), original.Joints.data(),
            vertexCount, sizeof(glm::u16vec4), indexRemap.data());
    if (!original.Weights.empty())
        meshopt_remapVertexBuffer(remapped.Weights.data(), original.Weights.data(),
            vertexCount, sizeof(glm::vec4), indexRemap.data());
}

void remapAttributesAsMeshlets(const std::vector<meshopt_Meshlet>& meshlets, const std::vector<u32>& meshletVertices,
    Attributes& remapped, Attributes& original)
{
    for (auto& meshlet : meshlets)
    {
        u32 vertexOffset = meshlet.vertex_offset;
        for (u32 localIndex = 0; localIndex < meshlet.vertex_count; localIndex++)
        {
            const u32 vertexIndex = vertexOffset + localIndex;
            const u32 remappedIndex = meshletVertices[vertexIndex];
            
            if (!original.Positions.empty())
                remapped.Positions[vertexIndex] = original.Positions[remappedIndex];
            if (!original.Normals.empty())
                remapped.Normals[vertexIndex] = original.Normals[remappedIndex];
            if (!original.Tangents.empty())
                remapped.Tangents[vertexIndex] = original.Tangents[remappedIndex];
            if (!original.UVs.empty())
                remapped.UVs[vertexIndex] = original.UVs[remappedIndex];
            if (!original.Joints.empty())
                remapped.Joints[vertexIndex] = original.Joints[remappedIndex];
            if (!original.Weights.empty())
                remapped.Weights[vertexIndex] = original.Weights[remappedIndex];
        }
    }
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
