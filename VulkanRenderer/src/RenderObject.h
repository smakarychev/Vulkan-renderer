#pragma once

#include <optional>
#include <glm/glm.hpp>

#include "Settings.h"
#include "Vulkan/Shader.h"

class DescriptorSet;
class Mesh;

struct Material
{
    // todo: find a better way?
    glm::vec4 Albedo;
    std::array<ShaderDescriptorSet, BUFFERED_FRAMES> DescriptorSets;
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