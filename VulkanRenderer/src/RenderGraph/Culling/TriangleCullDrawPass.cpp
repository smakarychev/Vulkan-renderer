#include "TriangleCullDrawPass.h"

#include "MeshletCullPass.h"

TriangleCullContext::TriangleCullContext(MeshletCullContext& meshletCullContext)
    : m_MeshletCullContext(&meshletCullContext)
{
    m_Visibility = Buffer::Builder()
        .SetSizeBytes((u64)(MAX_TRIANGLES * SUB_BATCH_COUNT * (u32)sizeof(RenderPassGeometry::TriangleVisibilityType)))
        .SetUsage(BufferUsage::Storage | BufferUsage::DeviceAddress)
        .Build();
}
