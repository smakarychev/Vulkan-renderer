#pragma once

#include "ShadowPassesCommon.h"
#include "Core/Camera.h"

namespace ShadowUtils
{
Camera shadowCameraStable(const FrustumCorners& frustumCorners, const AABB& geometryBounds,
    const glm::vec3& lightDirection, const glm::vec3& up);
Camera shadowCamera(const FrustumCorners& frustumCorners, const AABB& geometryBounds,
    const glm::vec3& lightDirection, const glm::vec3& up);
void stabilizeShadowProjection(Camera& camera, u32 shadowResolution);
}
