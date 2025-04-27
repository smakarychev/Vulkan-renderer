#pragma once

#include "types.h"
#include "Math/Geometry.h"

#include <glm/glm.hpp>

struct DirectionalLight;
class Camera;

struct ShadowProjectionBounds
{
    glm::vec3 Min{};
    glm::vec3 Max{};
    glm::vec3 Centroid{};
};