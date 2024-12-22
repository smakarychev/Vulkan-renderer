#pragma once

#include <glm/vec4.hpp>

/* this struct has to match the one defined in sh.glsl */

struct SH9Irradiance
{
    glm::vec4 AR;
    glm::vec4 AG;
    glm::vec4 AB;
    glm::vec4 BR;
    glm::vec4 BG;
    glm::vec4 BB;
    glm::vec4 C;
};
