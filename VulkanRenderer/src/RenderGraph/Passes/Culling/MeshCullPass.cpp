#include "MeshCullPass.h"

#include "Scene/SceneGeometry.h"

MeshCullContext::MeshCullContext(const SceneGeometry& geometry)
    : m_Geometry(&geometry)
{
    m_Visibility = Buffer::Builder({
            .SizeBytes = geometry.GetRenderObjectCount() * sizeof(SceneGeometry::ObjectVisibilityType),
            .Usage = BufferUsage::Storage | BufferUsage::DeviceAddress})
        .Build();
}
