#pragma once

#include "ShadowPassesCommon.h"
#include "Core/Camera.h"

namespace ShadowUtils
{
    void stabilizeShadowProjection(Camera& camera, u32 shadowResolution);

    ShadowProjectionBounds projectionBoundsSphereWorld(const FrustumCorners& frustumCorners);
}
