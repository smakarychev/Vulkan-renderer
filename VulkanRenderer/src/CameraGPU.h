#pragma once

#include "types.h"

#include <glm/glm.hpp>

/*
 * Used for most of the 'draw' passes of render graph
 * Reflected in `camera.glsl`
 */
struct CameraGPU
{
    glm::mat4 ViewProjection{glm::mat4{1.0f}};
    glm::mat4 Projection{glm::mat4{1.0f}};
    glm::mat4 View{glm::mat4{1.0f}};

    glm::vec3 Position{};
    f32 Near{0.1f};
    glm::vec3 Forward{glm::vec3{0.0f, 0.0f, 1.0f}};
    f32 Far{1000.0f};

    glm::mat4 InverseViewProjection{glm::mat4{1.0f}};
    glm::mat4 InverseProjection{glm::mat4{1.0f}};
    glm::mat4 InverseView{glm::mat4{1.0f}};

    glm::vec2 Resolution{1.0f}; 
};