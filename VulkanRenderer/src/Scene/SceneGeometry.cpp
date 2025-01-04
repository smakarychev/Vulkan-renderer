#include "SceneGeometry.h"

#include "Vulkan/Device.h"

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

    attributeBuffers.Positions = Device::CreateBuffer({
        .SizeBytes = totalPositionsSizeBytes,
        .Usage = vertexUsage});
    attributeBuffers.Normals = Device::CreateBuffer({
        .SizeBytes = totalNormalsSizeBytes,
        .Usage = vertexUsage});
    attributeBuffers.Tangents = Device::CreateBuffer({
        .SizeBytes = totalTangentsSizeBytes,
        .Usage = vertexUsage});
    attributeBuffers.UVs = Device::CreateBuffer({
        .SizeBytes = totalUVsSizeBytes,
        .Usage = vertexUsage});
    attributeBuffers.Indices = Device::CreateBuffer({
        .SizeBytes = totalIndicesSizeBytes,
        .Usage = indexUsage});
    Device::DeletionQueue().Enqueue(attributeBuffers.Positions);
    Device::DeletionQueue().Enqueue(attributeBuffers.Normals);
    Device::DeletionQueue().Enqueue(attributeBuffers.Tangents);
    Device::DeletionQueue().Enqueue(attributeBuffers.UVs);
    Device::DeletionQueue().Enqueue(attributeBuffers.Indices);

    return attributeBuffers;
}

void SceneGeometry::InitBuffers(SceneGeometry& renderPassGeometry, const CountsInfo& countsInfo)
{
    renderPassGeometry.m_CommandCount = countsInfo.CommandCount;
    renderPassGeometry.m_RenderObjectCount = countsInfo.RenderObjectCount;
    renderPassGeometry.m_Commands = Device::CreateBuffer({
        .SizeBytes = countsInfo.CommandCount * sizeof(IndirectDrawCommand),
        .Usage = BufferUsage::Ordinary | BufferUsage::Indirect | BufferUsage::Storage});
    renderPassGeometry.m_RenderObjects = Device::CreateBuffer({
        .SizeBytes = countsInfo.RenderObjectCount * sizeof(RenderObjectGPU),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage});
    renderPassGeometry.m_Materials = Device::CreateBuffer({
        .SizeBytes = countsInfo.RenderObjectCount * sizeof(MaterialGPU),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage});
    renderPassGeometry.m_Meshlets = Device::CreateBuffer({
        .SizeBytes = countsInfo.CommandCount * sizeof(MeshletGPU),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage});
    Device::DeletionQueue().Enqueue(renderPassGeometry.m_Commands);
    Device::DeletionQueue().Enqueue(renderPassGeometry.m_RenderObjects);
    Device::DeletionQueue().Enqueue(renderPassGeometry.m_Materials);
    Device::DeletionQueue().Enqueue(renderPassGeometry.m_Meshlets);

    renderPassGeometry.m_CommandsCPU.resize(countsInfo.CommandCount);
    renderPassGeometry.m_FirstCommands.resize(countsInfo.RenderObjectCount);
}
