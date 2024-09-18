#pragma once

#include <glm/vec3.hpp>

#include "GeometrySorter.h"
#include "types.h"
#include "Common/Geometry.h"

class ResourceUploader;

/* The primary use of this class is to sort translucent objects */
class DepthGeometrySorter final : GeometrySorter
{
public:
    DepthGeometrySorter(const glm::vec3& sortingPoint, const glm::vec3& sortingDirection);
    void Sort(SceneGeometry& geometry, ResourceUploader& resourceUploader) override;
private:
    Plane m_SortPlane{};
};
