#pragma once

#include <glm/glm.hpp>

#include "Vulkan/Pipeline.h"

class Mesh;

struct Material
{
    Pipeline Pipeline;
};

class RenderObject
{
public:
    Mesh* Mesh{nullptr};
    Material* Material{nullptr};
    glm::mat4 Transform{};
};
