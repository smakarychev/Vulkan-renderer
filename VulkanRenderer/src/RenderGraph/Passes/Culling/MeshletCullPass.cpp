#include "MeshletCullPass.h"

#include "MeshCullPass.h"

MeshletCullContext::MeshletCullContext(MeshCullContext& meshCullContext)
    : m_MeshCullContext(&meshCullContext)
{
    m_Visibility = Buffer::Builder({
            .SizeBytes = meshCullContext.Geometry().GetMeshletCount() *
                sizeof(RG::Geometry::MeshletVisibilityType),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();

    for (u32 i = 0; i < BUFFERED_FRAMES; i++)
    {
        m_CompactCount[i] = Buffer::Builder({
                .SizeBytes = sizeof(u32),
                .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress |
                    BufferUsage::Upload | BufferUsage::Readback})
            .Build();
        m_CompactCountReocclusion[i] = Buffer::Builder({
                .SizeBytes = sizeof(u32),
                .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress |
                    BufferUsage::Upload | BufferUsage::Readback})
            .Build();
    }
}

void MeshletCullContext::UpdateCompactCounts()
{
    m_FrameNumber = (m_FrameNumber + 1) % BUFFERED_FRAMES;
    
    m_CompactCountValues[m_FrameNumber] = ReadbackCount(m_CompactCount[m_FrameNumber]);
    m_CompactCountReocclusionValues[m_FrameNumber] = ReadbackCount(m_CompactCountReocclusion[m_FrameNumber]);
}

u32 MeshletCullContext::ReadbackCount(const Buffer& buffer) const
{
    const void* address = Driver::MapBuffer(buffer);
    u32 visibleMeshletsValue = *(const u32*)address;
    Driver::UnmapBuffer(buffer);

    return std::min(Geometry().GetMeshletCount(), visibleMeshletsValue);
}