#pragma once

#include "Mesh.h"
#include "ModelCollection.h"
#include "ResourceUploader.h"

namespace RG
{
    class Geometry
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
        static Geometry FromModelCollectionFiltered(const ModelCollection& modelCollection,
            ResourceUploader& resourceUploader, Filter&& filter);

        /* is called in geometry sorters */
        void ApplyRenderObjectPermutation(const ModelCollection::RenderObjectPermutation& permutation,
            ResourceUploader& resourceUploader); 

        const ModelCollection& GetModelCollection() const { return *m_ModelCollection; }
        const ModelCollection::RenderObjectIndices& GetRenderObjectIndices() const { return m_RenderObjectIndices; }
    
        const AttributeBuffers& GetAttributeBuffers() const { return m_AttributeBuffers; }
        const Buffer& GetCommandsBuffer() const { return m_Commands; }
        const Buffer& GetMaterialsBuffer() const { return m_Materials; }
        const Buffer& GetRenderObjectsBuffer() const { return m_RenderObjects; }
        const Buffer& GetMeshletsBuffer() const { return m_Meshlets; }

        u32 GetCommandCount() const { return m_CommandCount; }
        u32 GetMeshletCount() const { return m_CommandCount; }
        u32 GetRenderObjectCount() const { return m_RenderObjectCount; }

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
        static void InitBuffers(Geometry& renderPassGeometry, const CountsInfo& countsInfo);
    
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
    };

    template <typename Filter>
    Geometry Geometry::FromModelCollectionFiltered(const ModelCollection& modelCollection,
        ResourceUploader& resourceUploader, Filter&& filter)
    {
        CountsInfo countsInfo = GetCountsInfo(modelCollection, filter);
        Geometry renderPassGeometry = {};
        renderPassGeometry.m_ModelCollection = &modelCollection;
        renderPassGeometry.m_RenderObjectIndices.reserve(countsInfo.RenderObjectCount);
    
        if (countsInfo.RenderObjectCount == 0)
        {
            renderPassGeometry.m_CommandCount = 0;    
            renderPassGeometry.m_RenderObjectCount = 0;    
            renderPassGeometry.m_TriangleCount = 0;

            return renderPassGeometry;
        }
    
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
    
        auto populateCallback = [&](const RenderObject& renderObject, u32 collectionIndex)
        {
            renderPassGeometry.m_RenderObjectIndices.push_back(collectionIndex);
        
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

            renderPassGeometry.m_FirstCommands[renderObjectIndex] = meshletIndex;
        
            for (auto& meshlet : mesh.GetMeshlets())
            {
                IndirectDrawCommand command = {
                    .IndexCount = meshlet.IndexCount,
                    .InstanceCount = 1,
                    .FirstIndex = (u32)indicesOffset + meshlet.FirstIndex,
                    .VertexOffset = (i32)verticesOffset + (i32)meshlet.FirstVertex,
                    .FirstInstance = meshletIndex,
                    .RenderObject = renderObjectIndex};
                
                renderPassGeometry.m_CommandsCPU[meshletIndex] = command;
                
                commands[meshletIndex] = command;

                meshletsGPU[meshletIndex] = {
                    .BoundingCone = meshlet.BoundingCone,
                    .BoundingSphere = meshlet.BoundingSphere};
            
                meshletIndex++;
                renderPassGeometry.m_TriangleCount += meshlet.IndexCount / 3;
            }

            renderObjectsGPU[renderObjectIndex] = {
                .Transform = renderObject.Transform.ToMatrix(),
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
    Geometry::CountsInfo Geometry::GetCountsInfo(const ModelCollection& modelCollection,
        Filter&& filter)
    {
        CountsInfo countInfos = {};

        auto countCallback = [&modelCollection, &countInfos](const RenderObject& renderObject, u32)
        {
            countInfos.RenderObjectCount++;
            countInfos.CommandCount += modelCollection.GetMeshes()[renderObject.Mesh].GetMeshletCount();
            countInfos.VertexCount += modelCollection.GetMeshes()[renderObject.Mesh].GetVertexCount();
            countInfos.IndexCount += modelCollection.GetMeshes()[renderObject.Mesh].GetIndexCount();
        };

        modelCollection.FilterRenderObjects(filter, countCallback);

        return countInfos;
    }
}