#include "CullContext.h"

#include "Scene/SceneGeometry.h"
#include "Vulkan/Device.h"

CullViewVisibility::CullViewVisibility(const SceneGeometry& geometry)
    : m_Geometry(&geometry)
{
    m_Mesh = Device::CreateBuffer({
        .SizeBytes = geometry.GetRenderObjectCount() * sizeof(SceneGeometry::ObjectVisibilityType),
        .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress});

    m_Meshlet = Device::CreateBuffer({
        .SizeBytes = geometry.GetMeshletCount() * sizeof(SceneGeometry::MeshletVisibilityType),
        .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress}); 


    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        m_CompactCount[i] = Device::CreateBuffer({
            .SizeBytes = sizeof(u32),
            .Usage = BufferUsage::Ordinary | BufferUsage::Indirect | BufferUsage::Storage | BufferUsage::Readback});
    }
}

u32 CullViewVisibility::ReadbackCompactCountValue()
{
    m_CompactCountValue = ReadbackCount(m_CompactCount[PreviousFrame()]);
    
    return m_CompactCountValue;
}

u32 CullViewVisibility::ReadbackCount(Buffer buffer) const
{
    const void* address = Device::MapBuffer(buffer);
    u32 visibleMeshletsValue = *(const u32*)address;
    Device::UnmapBuffer(buffer);

    return std::min(m_Geometry->GetMeshletCount(), visibleMeshletsValue);
}

CullViewTriangleVisibility::CullViewTriangleVisibility(CullViewVisibility* cullViewVisibility)
    : m_CullViewVisibility(cullViewVisibility), m_Geometry(cullViewVisibility->Geometry())
{
    m_Triangle = Device::CreateBuffer({
        .SizeBytes = (u64)(m_Geometry->GetMeshletCount() * (u32)sizeof(SceneGeometry::TriangleVisibilityType)),
        .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress});
}

void CullViewTriangleVisibility::UpdateIterationCount()
{
    // todo: add some bias (like multiply by 1.25 for example)?
    u32 visibleMeshletsValue = ReadbackCompactCountValue();
    u32 commandCount = TriangleCullMultiviewTraits::CommandCount();
    m_BatchIterationCount = visibleMeshletsValue / commandCount + (u32)(visibleMeshletsValue % commandCount != 0);
}
