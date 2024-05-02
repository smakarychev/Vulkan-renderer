#pragma once

#include "types.h"

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
    /* DirectionalShadowPass will construct the suitable shadow camera based on main camera frustum */
    const Camera* MainCamera{nullptr};
    const DirectionalLight* DirectionalLight{nullptr};
    f32 ViewDistance{100};
};

struct ShadowProjectionBounds
{
    glm::vec3 Min{};
    glm::vec3 Max{};
    glm::vec3 Centroid{};
};