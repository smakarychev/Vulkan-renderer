#include "SceneGeometry.h"

void SceneGeometry::ApplyRenderObjectPermutation(const ModelCollection::RenderObjectPermutation& permutation,
    ResourceUploader& resourceUploader)
{
    IndirectDrawCommand* commands = resourceUploader.MapBuffer<IndirectDrawCommand>(m_Commands);

    u32 meshletIndex = 0;
    m_ModelCollection->IterateRenderObjects(m_RenderObjectIndices, permutation,
        [&](const RenderObject& renderObject, u32 renderObjectIndex)
        {
            const Mesh& mesh = m_ModelCollection->GetMeshes()[renderObject.Mesh];
            u32 firstCommand = m_FirstCommands[renderObjectIndex];
            
            for (u32 localIndex = 0; localIndex < mesh.GetMeshletCount(); localIndex++)
            {
                IndirectDrawCommand& command = m_CommandsCPU[firstCommand + localIndex];
                commands[meshletIndex] = command;
                
                meshletIndex++;
            }
        });      
}

SceneGeometry::AttributeBuffers SceneGeometry::InitAttributeBuffers(const CountsInfo& countsInfo)
{
    u64 totalPositionsSizeBytes = countsInfo.VertexCount * sizeof(glm::vec3);
    u64 totalNormalsSizeBytes = countsInfo.VertexCount * sizeof(glm::vec3);
    u64 totalTangentsSizeBytes = countsInfo.VertexCount * sizeof(glm::vec3);
    u64 totalUVsSizeBytes = countsInfo.VertexCount * sizeof(glm::vec2);
    u64 totalIndicesSizeBytes = countsInfo.IndexCount * sizeof(assetLib::ModelInfo::IndexType);

    BufferUsage vertexUsage =
        BufferUsage::Ordinary | BufferUsage::Vertex | BufferUsage::Storage;
    BufferUsage indexUsage =
        BufferUsage::Ordinary | BufferUsage::Index | BufferUsage::Storage;

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

void SceneGeometry::InitBuffers(SceneGeometry& renderPassGeometry, const CountsInfo& countsInfo)
{
    renderPassGeometry.m_CommandCount = countsInfo.CommandCount;
    renderPassGeometry.m_RenderObjectCount = countsInfo.RenderObjectCount;
    renderPassGeometry.m_Commands = Buffer::Builder({
            .SizeBytes = countsInfo.CommandCount * sizeof(IndirectDrawCommand),
            .Usage = BufferUsage::Ordinary | BufferUsage::Indirect | BufferUsage::Storage})
        .Build();
    renderPassGeometry.m_RenderObjects = Buffer::Builder({
            .SizeBytes = countsInfo.RenderObjectCount * sizeof(RenderObjectGPU),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage})
        .Build();
    renderPassGeometry.m_Materials = Buffer::Builder({
            .SizeBytes = countsInfo.RenderObjectCount * sizeof(MaterialGPU),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage})
        .Build();
    renderPassGeometry.m_Meshlets = Buffer::Builder({
            .SizeBytes = countsInfo.CommandCount * sizeof(MeshletGPU),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage})
        .Build();

    renderPassGeometry.m_CommandsCPU.resize(countsInfo.CommandCount);
    renderPassGeometry.m_FirstCommands.resize(countsInfo.RenderObjectCount);
}
