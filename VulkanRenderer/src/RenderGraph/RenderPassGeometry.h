#pragma once

#include "Mesh.h"
#include "ModelCollection.h"
#include "ResourceUploader.h"

class RenderPassGeometry
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
    using ObjectVisibilityType = u16;
    using MeshletVisibilityType = u16;
    using TriangleVisibilityType = u64;
public:
    template <typename Filter>
    static RenderPassGeometry FromModelCollectionFiltered(const ModelCollection& modelCollection,
        ResourceUploader& resourceUploader, Filter&& filter);

    const ModelCollection& GetModelCollection() const { return *m_ModelCollection; }
    
    const AttributeBuffers& GetAttributeBuffers() const { return m_AttributeBuffers; }
    const Buffer& GetCommandsBuffer() const { return m_Commands; }
    const Buffer& GetMaterialsBuffer() const { return m_Materials; }
    const Buffer& GetRenderObjectsBuffer() const { return m_RenderObjects; }
    const Buffer& GetMeshletsBuffer() const { return m_Meshlets; }

    u32 GetCommandCount() const { return m_CommandCount; }
    u32 GetMeshletCount() const { return m_CommandCount; }
    u32 GetRenderObjectCount() const { return m_RenderObjectCount; }
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
    static void InitBuffers(RenderPassGeometry& renderPassGeometry, const CountsInfo& countsInfo);
    
private:
    const ModelCollection* m_ModelCollection{nullptr};
    
    AttributeBuffers m_AttributeBuffers;
    Buffer m_Commands;
    Buffer m_Materials;
    Buffer m_RenderObjects;
    Buffer m_Meshlets;
    
    u32 m_CommandCount{};
    u32 m_RenderObjectCount{};
    u32 m_TriangleCount{};
};

template <typename Filter>
RenderPassGeometry RenderPassGeometry::FromModelCollectionFiltered(const ModelCollection& modelCollection,
    ResourceUploader& resourceUploader, Filter&& filter)
{
    CountsInfo countsInfo = GetCountsInfo(modelCollection, filter);

    RenderPassGeometry renderPassGeometry = {};

    renderPassGeometry.m_ModelCollection = &modelCollection;
    
    InitBuffers(renderPassGeometry, countsInfo);
    renderPassGeometry.m_AttributeBuffers = InitAttributeBuffers(countsInfo);

    u32 mappedCommandsBuffer = resourceUploader.GetMappedBuffer(renderPassGeometry.m_Commands.GetSizeBytes());
    IndirectDrawCommand* commands = (IndirectDrawCommand*)resourceUploader.GetMappedAddress(mappedCommandsBuffer);

    u32 mappedMeshletsBuffer = resourceUploader.GetMappedBuffer(renderPassGeometry.m_Meshlets.GetSizeBytes());
    MeshletGPU* meshletsGPU = (MeshletGPU*)resourceUploader.GetMappedAddress(mappedMeshletsBuffer);

    u32 mappedMaterialsBuffer = resourceUploader.GetMappedBuffer(renderPassGeometry.m_Materials.GetSizeBytes());
    MaterialGPU* materialsGPU = (MaterialGPU*)resourceUploader.GetMappedAddress(mappedMaterialsBuffer);

    u32 mappedObjectsBuffer = resourceUploader.GetMappedBuffer(renderPassGeometry.m_RenderObjects.GetSizeBytes());
    RenderObjectGPU* renderObjectsGPU = (RenderObjectGPU*)resourceUploader.GetMappedAddress(mappedObjectsBuffer);

    u64 verticesOffset = 0;
    u64 indicesOffset = 0;
    u32 renderObjectIndex = 0;
    u32 meshletIndex = 0;
    
    auto populateCallback = [&](const RenderObject& renderObject)
    {
        const Mesh& mesh = modelCollection.GetMeshes()[renderObject.Mesh];

        u64 verticesSize = mesh.GetPositions().size();
        u64 indicesSize = mesh.GetIndices().size();
        resourceUploader.UpdateBuffer(renderPassGeometry.m_AttributeBuffers.Positions, mesh.GetPositions().data(),
            verticesSize * sizeof(glm::vec3),
            verticesOffset * sizeof(glm::vec3));
        resourceUploader.UpdateBuffer(renderPassGeometry.m_AttributeBuffers.Normals, mesh.GetNormals().data(),
            verticesSize * sizeof(glm::vec3),
            verticesOffset * sizeof(glm::vec3));
        resourceUploader.UpdateBuffer(renderPassGeometry.m_AttributeBuffers.Tangents, mesh.GetTangents().data(),
            verticesSize * sizeof(glm::vec3),
            verticesOffset * sizeof(glm::vec3));
        resourceUploader.UpdateBuffer(renderPassGeometry.m_AttributeBuffers.UVs, mesh.GetUVs().data(),
            verticesSize * sizeof(glm::vec2),
            verticesOffset * sizeof(glm::vec2));
        resourceUploader.UpdateBuffer(renderPassGeometry.m_AttributeBuffers.Indices, mesh.GetIndices().data(),
            indicesSize * sizeof(assetLib::ModelInfo::IndexType),
            indicesOffset * sizeof(assetLib::ModelInfo::IndexType));
        
        for (auto& meshlet : mesh.GetMeshlets())
        {
            commands[meshletIndex].FirstIndex = (u32)indicesOffset + meshlet.FirstIndex;
            commands[meshletIndex].IndexCount = meshlet.IndexCount;
            commands[meshletIndex].FirstInstance = meshletIndex;
            commands[meshletIndex].InstanceCount = 1;
            commands[meshletIndex].VertexOffset = (i32)verticesOffset + (i32)meshlet.FirstVertex;
            commands[meshletIndex].RenderObject = renderObjectIndex;

            meshletsGPU[meshletIndex] = {
                .BoundingCone = meshlet.BoundingCone,
                .BoundingSphere = meshlet.BoundingSphere};
            
            meshletIndex++;
            renderPassGeometry.m_TriangleCount += meshlet.IndexCount / 3;
        }

        renderObjectsGPU[renderObjectIndex] = {
            .Transform = renderObject.Transform,
            .BoundingSphere = mesh.GetBoundingSphere()};

        materialsGPU[renderObjectIndex] =
            modelCollection.GetMaterialsGPU()[renderObject.MaterialGPU];
        
        verticesOffset += verticesSize;
        indicesOffset += indicesSize;
        renderObjectIndex++;
    };

    modelCollection.FilterRenderObjects(filter, populateCallback);
    
    resourceUploader.UpdateBuffer(renderPassGeometry.m_Commands, mappedCommandsBuffer, 0);
    resourceUploader.UpdateBuffer(renderPassGeometry.m_Meshlets, mappedMeshletsBuffer, 0);
    resourceUploader.UpdateBuffer(renderPassGeometry.m_RenderObjects, mappedObjectsBuffer, 0);
    resourceUploader.UpdateBuffer(renderPassGeometry.m_Materials, mappedMaterialsBuffer, 0);

    return renderPassGeometry;
}

template <typename Filter>
RenderPassGeometry::CountsInfo RenderPassGeometry::GetCountsInfo(const ModelCollection& modelCollection,
    Filter&& filter)
{
    CountsInfo countInfos = {};

    auto countCallback = [&modelCollection, &countInfos](const RenderObject& renderObject)
    {
        countInfos.RenderObjectCount++;
        countInfos.CommandCount += modelCollection.GetMeshes()[renderObject.Mesh].GetMeshletCount();
        countInfos.VertexCount += modelCollection.GetMeshes()[renderObject.Mesh].GetVertexCount();
        countInfos.IndexCount += modelCollection.GetMeshes()[renderObject.Mesh].GetIndexCount();
    };

    modelCollection.FilterRenderObjects(filter, countCallback);

    return countInfos;
}

inline RenderPassGeometry::AttributeBuffers RenderPassGeometry::InitAttributeBuffers(const CountsInfo& countsInfo)
{
    u64 totalPositionsSizeBytes = countsInfo.VertexCount * sizeof(glm::vec3);
    u64 totalNormalsSizeBytes = countsInfo.VertexCount * sizeof(glm::vec3);
    u64 totalTangentsSizeBytes = countsInfo.VertexCount * sizeof(glm::vec3);
    u64 totalUVsSizeBytes = countsInfo.VertexCount * sizeof(glm::vec2);
    u64 totalIndicesSizeBytes = countsInfo.IndexCount * sizeof(assetLib::ModelInfo::IndexType);

    BufferUsage vertexUsage =
        BufferUsage::Vertex | BufferUsage::Storage | BufferUsage::Destination | BufferUsage::DeviceAddress;
    BufferUsage indexUsage =
        BufferUsage::Index | BufferUsage::Storage | BufferUsage::Destination | BufferUsage::DeviceAddress;

    AttributeBuffers attributeBuffers = {};

    attributeBuffers.Positions = Buffer::Builder({.SizeBytes = totalPositionsSizeBytes, .Usage = vertexUsage})
        .Build();
    attributeBuffers.Normals = Buffer::Builder({.SizeBytes = totalNormalsSizeBytes, .Usage = vertexUsage})
        .Build();
    attributeBuffers.Tangents = Buffer::Builder({.SizeBytes = totalTangentsSizeBytes, .Usage = vertexUsage})
        .Build();
    attributeBuffers.UVs = Buffer::Builder({.SizeBytes = totalUVsSizeBytes, .Usage = vertexUsage})
        .Build();
    attributeBuffers.Indices = Buffer::Builder({.SizeBytes = totalIndicesSizeBytes, .Usage = indexUsage})
        .Build();

    return attributeBuffers;
}

inline void RenderPassGeometry::InitBuffers(RenderPassGeometry& renderPassGeometry, const CountsInfo& countsInfo)
{
    renderPassGeometry.m_CommandCount = countsInfo.CommandCount;
    renderPassGeometry.m_RenderObjectCount = countsInfo.RenderObjectCount;
    renderPassGeometry.m_Commands = Buffer::Builder({
            .SizeBytes = countsInfo.CommandCount * sizeof(IndirectDrawCommand),
            .Usage = BufferUsage::Indirect | BufferUsage::Storage | BufferUsage::Destination |
                BufferUsage::DeviceAddress})
        .Build();
    renderPassGeometry.m_RenderObjects = Buffer::Builder({
            .SizeBytes = countsInfo.RenderObjectCount * sizeof(RenderObjectGPU),
            .Usage = BufferUsage::Storage | BufferUsage::Destination | BufferUsage::DeviceAddress})
        .Build();
    renderPassGeometry.m_Materials = Buffer::Builder({
            .SizeBytes = countsInfo.RenderObjectCount * sizeof(MaterialGPU),
            .Usage = BufferUsage::Storage | BufferUsage::Destination | BufferUsage::DeviceAddress})
       .Build();
    renderPassGeometry.m_Meshlets = Buffer::Builder({
            .SizeBytes = countsInfo.CommandCount * sizeof(MeshletGPU),
            .Usage = BufferUsage::Storage | BufferUsage::Destination | BufferUsage::DeviceAddress})
        .Build();
}
