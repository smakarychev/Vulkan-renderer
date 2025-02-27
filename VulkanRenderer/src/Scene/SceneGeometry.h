#pragma once

#include "Mesh.h"
#include "ModelCollection.h"
#include "ResourceUploader.h"

class SceneGeometry
{
public:
    struct AttributeBuffers
    {
        Buffer Positions;
        Buffer Normals;
        Buffer Tangents;
        Buffer UVs;
        Buffer Indices;
    };
    using ObjectVisibilityType = u8;
    using MeshletVisibilityType = u8;
    using TriangleVisibilityType = u64;
public:
    template <typename Filter>
    static SceneGeometry FromModelCollectionFiltered(const ModelCollection& modelCollection,
        ResourceUploader& resourceUploader, Filter&& filter);

    /* is called in geometry sorters */
    void ApplyRenderObjectPermutation(const ModelCollection::RenderObjectPermutation& permutation,
        ResourceUploader& resourceUploader); 

    const ModelCollection& GetModelCollection() const { return *m_ModelCollection; }
    const ModelCollection::RenderObjectIndices& GetRenderObjectIndices() const { return m_RenderObjectIndices; }

    const AttributeBuffers& GetAttributeBuffers() const { return m_AttributeBuffers; }
    Buffer GetCommandsBuffer() const { return m_Commands; }
    Buffer GetMaterialsBuffer() const { return m_Materials; }
    Buffer GetRenderObjectsBuffer() const { return m_RenderObjects; }
    Buffer GetMeshletsBuffer() const { return m_Meshlets; }

    u32 GetCommandCount() const { return m_CommandCount; }
    u32 GetMeshletCount() const { return m_CommandCount; }
    u32 GetRenderObjectCount() const { return m_RenderObjectCount; }
    u32 GetTriangleCount() const { return m_TriangleCount; }

    const AABB& GetBounds() const { return m_Bounds; }

    bool IsValid() const { return m_CommandCount != 0; }
private:
    struct CountsInfo
    {
        u32 RenderObjectCount;
        u32 CommandCount;
        u32 VertexCount;
        u32 IndexCount;
    };

    template <typename Filter>
    static CountsInfo GetCountsInfo(const ModelCollection& modelCollection, Filter&& filter);

    static AttributeBuffers InitAttributeBuffers(const CountsInfo& countsInfo);
    static void InitBuffers(SceneGeometry& renderPassGeometry, const CountsInfo& countsInfo);

private:
    const ModelCollection* m_ModelCollection{nullptr};
    ModelCollection::RenderObjectIndices m_RenderObjectIndices;

    AttributeBuffers m_AttributeBuffers;
    Buffer m_Commands;
    Buffer m_Materials;
    Buffer m_RenderObjects;
    Buffer m_Meshlets;

    /* Cpu-side of `m_Commands` buffer, used for reordering and updating */
    std::vector<IndirectDrawCommand> m_CommandsCPU;
    /* index of the first command for each render object, used for reordering and updating */
    std::vector<u32> m_FirstCommands;

    u32 m_CommandCount{};
    u32 m_RenderObjectCount{};
    u32 m_TriangleCount{};

    AABB m_Bounds{};
};

template <typename Filter>
SceneGeometry SceneGeometry::FromModelCollectionFiltered(const ModelCollection& modelCollection,
    ResourceUploader& resourceUploader, Filter&& filter)
{
    CountsInfo countsInfo = GetCountsInfo(modelCollection, filter);
    SceneGeometry sceneGeometry = {};
    sceneGeometry.m_ModelCollection = &modelCollection;
    sceneGeometry.m_Bounds = modelCollection.GetBounds();
    sceneGeometry.m_RenderObjectIndices.reserve(countsInfo.RenderObjectCount);

    if (countsInfo.RenderObjectCount == 0)
    {
        sceneGeometry.m_CommandCount = 0;    
        sceneGeometry.m_RenderObjectCount = 0;    
        sceneGeometry.m_TriangleCount = 0;

        return sceneGeometry;
    }

    InitBuffers(sceneGeometry, countsInfo);
    sceneGeometry.m_AttributeBuffers = InitAttributeBuffers(countsInfo);

    IndirectDrawCommand* commands = resourceUploader.MapBuffer<IndirectDrawCommand>(sceneGeometry.m_Commands);
    MeshletGPU* meshletsGPU = resourceUploader.MapBuffer<MeshletGPU>(sceneGeometry.m_Meshlets);
    MaterialGPU* materialsGPU = resourceUploader.MapBuffer<MaterialGPU>(sceneGeometry.m_Materials);
    RenderObjectGPU* renderObjectsGPU = resourceUploader.MapBuffer<RenderObjectGPU>(sceneGeometry.m_RenderObjects);

    u64 verticesOffset = 0;
    u64 indicesOffset = 0;
    u32 renderObjectIndex = 0;
    u32 meshletIndex = 0;

    std::vector verticesOffsets(modelCollection.GetMeshes().size(), 0llu);
    std::vector indicesOffsets(modelCollection.GetMeshes().size(), 0llu);
    auto meshCallback = [&](const Mesh& mesh, u32 meshIndex)
    {
        u64 verticesSize = mesh.GetPositions().size();
        u64 indicesSize = mesh.GetIndices().size();
        resourceUploader.UpdateBuffer(sceneGeometry.m_AttributeBuffers.Positions, mesh.GetPositions(),
            verticesOffset * sizeof(glm::vec3));
        resourceUploader.UpdateBuffer(sceneGeometry.m_AttributeBuffers.Normals, mesh.GetNormals(),
            verticesOffset * sizeof(glm::vec3));
        resourceUploader.UpdateBuffer(sceneGeometry.m_AttributeBuffers.Tangents, mesh.GetTangents(),
            verticesOffset * sizeof(glm::vec4));
        resourceUploader.UpdateBuffer(sceneGeometry.m_AttributeBuffers.UVs, mesh.GetUVs(),
            verticesOffset * sizeof(glm::vec2));
        resourceUploader.UpdateBuffer(sceneGeometry.m_AttributeBuffers.Indices, mesh.GetIndices(),
            indicesOffset * sizeof(assetLib::ModelInfo::IndexType));

        verticesOffsets[meshIndex] = verticesOffset;
        indicesOffsets[meshIndex] = indicesOffset;
        
        verticesOffset += verticesSize;
        indicesOffset += indicesSize;
    };
    auto renderObjectCallback = [&](const RenderObject& renderObject, u32 collectionIndex)
    {
        sceneGeometry.m_RenderObjectIndices.push_back(collectionIndex);
    
        const Mesh& mesh = modelCollection.GetMeshes()[renderObject.Mesh];
        u32 meshIndex = modelCollection.GetMeshes().index_of(renderObject.Mesh);

        sceneGeometry.m_FirstCommands[renderObjectIndex] = meshletIndex;
    
        for (auto& meshlet : mesh.GetMeshlets())
        {
            IndirectDrawCommand command = {
                .IndexCount = meshlet.IndexCount,
                .InstanceCount = 1,
                .FirstIndex = (u32)indicesOffsets[meshIndex] + meshlet.FirstIndex,
                .VertexOffset = (i32)verticesOffsets[meshIndex] + (i32)meshlet.FirstVertex,
                .FirstInstance = meshletIndex,
                .RenderObject = renderObjectIndex};
            
            sceneGeometry.m_CommandsCPU[meshletIndex] = command;
            
            commands[meshletIndex] = command;

            meshletsGPU[meshletIndex] = {
                .BoundingCone = meshlet.BoundingCone,
                .BoundingSphere = {.Center = meshlet.BoundingSphere.Center, .Radius = meshlet.BoundingSphere.Radius}};
        
            meshletIndex++;
            sceneGeometry.m_TriangleCount += meshlet.IndexCount / 3;
        }

        renderObjectsGPU[renderObjectIndex] = {
            .Transform = renderObject.Transform.ToMatrix(),
            .BoundingSphere = mesh.GetBoundingSphere()};

        materialsGPU[renderObjectIndex] =
            modelCollection.GetMaterialsGPU()[renderObject.MaterialGPU];
        
        renderObjectIndex++;
    };

    modelCollection.FilterMeshes(filter, meshCallback);
    modelCollection.FilterRenderObjects(filter, renderObjectCallback);

    return sceneGeometry;
}

template <typename Filter>
SceneGeometry::CountsInfo SceneGeometry::GetCountsInfo(const ModelCollection& modelCollection,
    Filter&& filter)
{
    CountsInfo countInfos = {};

    auto meshCallback = [&countInfos](const Mesh& mesh, u32)
    {
        countInfos.VertexCount += mesh.GetVertexCount();
        countInfos.IndexCount += mesh.GetIndexCount();
    };
    auto renderObjectCallback = [&modelCollection, &countInfos](const RenderObject& renderObject, u32)
    {
        countInfos.RenderObjectCount++;
        countInfos.CommandCount += modelCollection.GetMeshes()[renderObject.Mesh].GetMeshletCount();
    };

    modelCollection.FilterMeshes(filter, meshCallback);
    modelCollection.FilterRenderObjects(filter, renderObjectCallback);

    return countInfos;
}