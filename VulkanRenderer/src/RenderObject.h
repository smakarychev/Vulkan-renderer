#pragma once

#include <optional>
#include <glm/glm.hpp>

#include "Vulkan/Shader.h"

class DescriptorSet;
class Mesh;

struct Material
{
    // todo: find a better way?
    std::optional<ShaderDescriptorSet> TextureSet;
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