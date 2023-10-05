#pragma once

#include <glm/glm.hpp>

#include "Settings.h"
#include "Vulkan/Shader.h"

class DescriptorSet;
class Mesh;

struct Material
{
    // todo: find a better way?
    glm::vec4 Albedo;
    ShaderDescriptorSet DescriptorSet;
    ShaderPipeline Pipeline;
};

class RenderObject
{
public:
    Mesh* Mesh{nullptr};
    Material* Material{nullptr};
    glm::mat4 Transform{};
};

struct BatchIndirect
{
    Mesh* Mesh{nullptr};
    Material* Material{nullptr};
    u32 First{0};
    u32 Count{0};
};