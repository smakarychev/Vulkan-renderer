#include "MeshCullPass.h"

#include "..\RGGeometry.h"

MeshCullContext::MeshCullContext(const RenderPassGeometry& geometry)
    : m_Geometry(&geometry)
{
    m_Visibility = Buffer::Builder({
            .SizeBytes = geometry.GetRenderObjectCount() * sizeof(RenderPassGeometry::ObjectVisibilityType),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();
}
