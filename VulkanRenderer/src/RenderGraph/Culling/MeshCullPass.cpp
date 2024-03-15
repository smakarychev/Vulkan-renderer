#include "MeshCullPass.h"

#include "RenderGraph/RenderPassGeometry.h"

MeshCullContext::MeshCullContext(const RenderPassGeometry& geometry)
    : m_Geometry(&geometry)
{
    m_Visibility = Buffer::Builder()
        .SetSizeBytes(geometry.GetRenderObjectCount() * sizeof(RenderPassGeometry::ObjectVisibilityType))
        .SetUsage(BufferUsage::Storage | BufferUsage::DeviceAddress)
        .Build();
}
