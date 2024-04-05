#include "TriangleCullDrawPass.h"

#include "MeshletCullPass.h"

TriangleCullContext::TriangleCullContext(MeshletCullContext& meshletCullContext)
    : m_MeshletCullContext(&meshletCullContext)
{
    m_Visibility = Buffer::Builder({
            .SizeBytes = (u64)(MAX_TRIANGLES * SUB_BATCH_COUNT *
                (u32)sizeof(RG::Geometry::TriangleVisibilityType)),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();
}
