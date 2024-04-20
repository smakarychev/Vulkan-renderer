#pragma once

#include "types.h"

#include <glm/glm.hpp>

struct DirectionalLight
{
    glm::vec3 Direction{0.0f, -1.0f, 0.0f};
    glm::vec3 Color{1.0f};
    f32 Intensity{1.0f};
};
