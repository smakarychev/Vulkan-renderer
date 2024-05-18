#include "CullContext.h"

#include "Scene/SceneGeometry.h"
#include "Vulkan/Driver.h"

CullViewVisibility::CullViewVisibility(const SceneGeometry& geometry)
    : m_Geometry(&geometry)
{
    m_Mesh = Buffer::Builder({
            .SizeBytes = geometry.GetRenderObjectCount() * sizeof(SceneGeometry::ObjectVisibilityType),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();

    m_Meshlet = Buffer::Builder({
           .SizeBytes = geometry.GetMeshletCount() * sizeof(SceneGeometry::MeshletVisibilityType),
           .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
       .Build();

    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
        m_CompactCount[i] = Buffer::Builder({
                .SizeBytes = sizeof(u32),
                .Usage = BufferUsage::Indirect | BufferUsage::Storage | BufferUsage::DeviceAddress |
                    BufferUsage::Upload | BufferUsage::Readback})
            .Build();
}

u32 CullViewVisibility::ReadbackCompactCountValue()
{
    m_CompactCountValue = ReadbackCount(m_CompactCount[PreviousFrame()]);
    
    return m_CompactCountValue;
}

u32 CullViewVisibility::ReadbackCount(const Buffer& buffer) const
{
    const void* address = Driver::MapBuffer(buffer);
    u32 visibleMeshletsValue = *(const u32*)address;
    Driver::UnmapBuffer(buffer);

    return std::min(m_Geometry->GetMeshletCount(), visibleMeshletsValue);
}

CullViewTriangleVisibility::CullViewTriangleVisibility(CullViewVisibility* cullViewVisibility)
    : m_CullViewVisibility(cullViewVisibility), m_Geometry(cullViewVisibility->Geometry())
{
    m_Triangle = Buffer::Builder({
            .SizeBytes = (u64)(m_Geometry->GetMeshletCount() *
                (u32)sizeof(SceneGeometry::TriangleVisibilityType)),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();
}