#include "MeshletCullPass.h"

#include "MeshCullPass.h"

MeshletCullContext::MeshletCullContext(MeshCullContext& meshCullContext)
    : m_MeshCullContext(&meshCullContext)
{
    m_Visibility = Buffer::Builder()
        .SetSizeBytes(meshCullContext.Geometry().GetMeshletCount() * sizeof(RenderPassGeometry::MeshletVisibilityType))
        .SetUsage(BufferUsage::Storage | BufferUsage::DeviceAddress)
        .Build();
}
