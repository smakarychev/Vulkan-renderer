#pragma once

#include <glm/vec3.hpp>

#include "RGGeometrySorter.h"
#include "types.h"

class ResourceUploader;

namespace RG
{
    /* The primary use of this class is to sort translucent objects */
    class DepthGeometrySorter final : GeometrySorter
    {
    public:
        DepthGeometrySorter(const glm::vec3& sortingPoint, const glm::vec3& sortingDirection);
        void Sort(Geometry& geometry, ResourceUploader& resourceUploader);
    private:
        f32 m_PlaneOffset;
        glm::vec3 m_PlaneNormal{};
    };
}
