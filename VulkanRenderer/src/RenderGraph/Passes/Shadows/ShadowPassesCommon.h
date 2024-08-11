#pragma once

#include "types.h"
#include "Common/Geometry.h"

#include <glm/glm.hpp>

struct DirectionalLight;
class Camera;
class SceneGeometry;

struct ShadowPassInitInfo
{
    const SceneGeometry* Geometry{nullptr};
};

struct ShadowPassExecutionInfo
{
    const SceneGeometry* Geometry{nullptr};
    /* pass will construct the suitable shadow camera based on main camera frustum */
    const Camera* MainCamera{nullptr};
    const DirectionalLight* DirectionalLight{nullptr};
    f32 ShadowMin{0};
    f32 ShadowMax{100};
    bool StabilizeCascades{false};
    AABB GeometryBounds{};
};

struct ShadowProjectionBounds
{
    glm::vec3 Min{};
    glm::vec3 Max{};
    glm::vec3 Centroid{};
};