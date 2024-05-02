#include "DepthGeometrySorter.h"

#include <ranges>
#include <algorithm>
#include <numeric>

#include "Scene/SceneGeometry.h"

DepthGeometrySorter::DepthGeometrySorter(const glm::vec3& sortingPoint, const glm::vec3& sortingDirection)
    : m_PlaneOffset(-glm::dot(sortingDirection, sortingPoint)), m_PlaneNormal(sortingDirection)
{
}

void DepthGeometrySorter::Sort(SceneGeometry& geometry, ResourceUploader& resourceUploader)
{
    std::vector<f32> depths;
    depths.reserve(geometry.GetRenderObjectCount());
    
    geometry.GetModelCollection().IterateRenderObjects(geometry.GetRenderObjectIndices(),
        [this, &geometry, &depths](const RenderObject& renderObject, u32)
        {
            const Mesh& mesh = geometry.GetModelCollection().GetMeshes()[renderObject.Mesh];
            f32 planeDistance = glm::dot(m_PlaneNormal, mesh.GetBoundingSphere().Center) + m_PlaneOffset;
            /* we do not care about negative distance, clamping it at 0 keeps the order of every
             * object behind the sorting plane
             */
            depths.push_back(std::max(0.0f, planeDistance));
        });


    std::vector<u32> indexPermutation(geometry.GetRenderObjectCount());
    std::iota(indexPermutation.begin(), indexPermutation.end(), 0);
    std::ranges::sort(
        std::ranges::views::zip(depths, indexPermutation),
        std::greater<>{},
        [](const auto& elem) { return std::get<0>(elem); });

    geometry.ApplyRenderObjectPermutation(indexPermutation, resourceUploader);
}
