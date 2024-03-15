#include "MeshletCullPass.h"

#include "MeshCullPass.h"

MeshletCullContext::MeshletCullContext(const MeshCullContext& meshCullContext)
    : m_Geometry(&meshCullContext.Geometry())
{
    m_Visibility = Buffer::Builder()
        .SetSizeBytes(m_Geometry->GetMeshletCount() * sizeof(RenderPassGeometry::MeshletVisibilityType))
        .SetUsage(BufferUsage::Storage | BufferUsage::DeviceAddress)
        .Build();
}
