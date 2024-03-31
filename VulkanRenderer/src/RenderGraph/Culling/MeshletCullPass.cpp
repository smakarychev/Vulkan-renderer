#include "MeshletCullPass.h"

#include "MeshCullPass.h"

MeshletCullContext::MeshletCullContext(MeshCullContext& meshCullContext)
    : m_MeshCullContext(&meshCullContext)
{
    m_Visibility = Buffer::Builder({
            .SizeBytes = meshCullContext.Geometry().GetMeshletCount() *
                sizeof(RenderPassGeometry::MeshletVisibilityType),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();
}
