#pragma once

#include "types.h"

#include <glm/glm.hpp>

struct DirectionalLight;
class Camera;
namespace RG
{
    class Geometry;
}

struct ShadowPassInitInfo
{
    const RG::Geometry* Geometry{nullptr};
};

struct ShadowPassExecutionInfo
{
    glm::uvec2 Resolution{};
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