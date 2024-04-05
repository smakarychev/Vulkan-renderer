#include "MeshCullPass.h"

#include "RenderGraph/RGGeometry.h"

MeshCullContext::MeshCullContext(const RG::Geometry& geometry)
    : m_Geometry(&geometry)
{
    m_Visibility = Buffer::Builder({
            .SizeBytes = geometry.GetRenderObjectCount() * sizeof(RG::Geometry::ObjectVisibilityType),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();
}
