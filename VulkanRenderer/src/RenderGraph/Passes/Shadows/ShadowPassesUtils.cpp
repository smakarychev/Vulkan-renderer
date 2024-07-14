#include "ShadowPassesUtils.h"

#include <array>

namespace ShadowUtils
{
    void stabilizeShadowProjection(Camera& camera, u32 shadowResolution)
    {
        glm::mat4 shadowMatrix = camera.GetViewProjection();
        glm::vec4 shadowOrigin = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
        /* transform the world origin to shadow-projected space, and scale by half resolution, to
         * get into the pixel space;
         * then snap to the closest pixel and transform the offset back to world space units.
         * this also scales z and w coordinate, but it is ignored */
        shadowOrigin = shadowMatrix * shadowOrigin * ((f32)shadowResolution / 2.0f);
        glm::vec4 shadowOriginRounded = glm::round(shadowOrigin);
        glm::vec4 roundingOffset = shadowOriginRounded - shadowOrigin;
        roundingOffset.z = roundingOffset.w = 0.0f;
        roundingOffset *= 2.0f / (f32)shadowResolution;
        glm::mat4 stabilizedProjection = camera.GetProjection();
        stabilizedProjection[3] += roundingOffset;
        camera.SetProjection(stabilizedProjection);
    }

    ShadowProjectionBounds projectionBoundsSphereWorld(const FrustumCorners& frustumCorners, const AABB& geometryBounds)
    {
        glm::vec3 centroid = {};
        for (auto& p : frustumCorners)
            centroid += p;
        centroid /= (f32)frustumCorners.size();
    
        f32 boundingSphereRadius = 0.0f;
        for (auto& p : frustumCorners)
            boundingSphereRadius = std::max(boundingSphereRadius, glm::distance2(p, centroid));
        boundingSphereRadius = std::sqrt(boundingSphereRadius);

        f32 geometryRadius = glm::distance((geometryBounds.Max + geometryBounds.Min) * 0.5f, geometryBounds.Max);

        if (geometryRadius < boundingSphereRadius)
        {
            boundingSphereRadius = geometryRadius;
            centroid = (geometryBounds.Max + geometryBounds.Min) * 0.5f;
        }
        
        static constexpr f32 RADIUS_SNAP = 16.0f;
        boundingSphereRadius = std::ceil(boundingSphereRadius * RADIUS_SNAP) / RADIUS_SNAP;

        glm::vec3 max = glm::vec3{boundingSphereRadius};
        glm::vec3 min = -max;

        return {
            .Min = min,
            .Max = max,
            .Centroid = centroid};
    }
}


