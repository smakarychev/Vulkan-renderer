#include "ShadowPassesUtils.h"

#include <array>

#include "Settings.h"

namespace
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
            //boundingSphereRadius = geometryRadius;
            //centroid = (geometryBounds.Max + geometryBounds.Min) * 0.5f;
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

namespace ShadowUtils
{
    Camera shadowCameraStable(const FrustumCorners& frustumCorners, const AABB& geometryBounds,
        const glm::vec3& lightDirection, const glm::vec3& up)
    {
        ShadowProjectionBounds bounds = projectionBoundsSphereWorld(frustumCorners, geometryBounds);

        // todo: I believe it no longer
        /* pcss method does not like 0 on a near plane */
        static constexpr f32 NEAR_RELATIVE_OFFSET = 0.1f;

        f32 cameraCentroidOffset = bounds.Min.z * (1.0f + NEAR_RELATIVE_OFFSET);
        glm::vec3 cameraPosition = bounds.Centroid + lightDirection * cameraCentroidOffset;
        Camera shadowCamera = Camera::Orthographic({
            .BaseInfo = {
                .Position = cameraPosition,
                .Orientation = glm::normalize(glm::quatLookAt(lightDirection, up)),
                .Near = -bounds.Min.z * NEAR_RELATIVE_OFFSET,
                .Far = bounds.Max.z - cameraCentroidOffset,
                .ViewportWidth = SHADOW_MAP_RESOLUTION,
                .ViewportHeight = SHADOW_MAP_RESOLUTION},
            .Left = bounds.Min.x,
            .Right = bounds.Max.x,
            .Bottom = bounds.Min.y,
            .Top = bounds.Max.y});

        /* stabilize the camera */
        stabilizeShadowProjection(shadowCamera, SHADOW_MAP_RESOLUTION);

        return shadowCamera;
    }

    Camera shadowCamera(const FrustumCorners& frustumCorners, const AABB& geometryBounds,
        const glm::vec3& lightDirection, const glm::vec3& up)
    {
        glm::vec3 centroid = {};
        for (auto& p : frustumCorners)
            centroid += p;
        centroid /= (f32)frustumCorners.size();

        glm::mat4 lightView = glm::lookAtRH(centroid, centroid + lightDirection, up);

        glm::vec3 min = glm::vec3{std::numeric_limits<f32>::max()};
        glm::vec3 max = -min;
        for (auto& c : frustumCorners)
        {
            min = glm::min(min, glm::vec3{lightView * glm::vec4{c, 1.0f}});
            max = glm::max(max, glm::vec3{lightView * glm::vec4{c, 1.0f}});
        }
        f32 scale = ((f32)SHADOW_MAP_RESOLUTION + 7.0f) / (f32)SHADOW_MAP_RESOLUTION;
        min.x *= scale;
        min.y *= scale;
        max.x *= scale;
        max.y *= scale;

        glm::vec3 cascadeExtents = max - min;
        glm::vec3 shadowPosition = centroid - lightDirection * max.z;
        Camera shadowCamera = Camera::Orthographic({
            .BaseInfo = {
                .Position = shadowPosition,
                .Orientation = glm::normalize(glm::quatLookAt(lightDirection, up)),
                .Near = 0.0f,
                .Far = cascadeExtents.z,
                .ViewportWidth = SHADOW_MAP_RESOLUTION,
                .ViewportHeight = SHADOW_MAP_RESOLUTION},
            .Left = min.x,
            .Right = max.x,
            .Bottom = min.y,
            .Top = max.y});

        return shadowCamera;
    }
}


