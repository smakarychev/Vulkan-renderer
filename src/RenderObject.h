#pragma once

#include <optional>
#include <glm/glm.hpp>

#include "Vulkan/Pipeline.h"
#include "Vulkan/DescriptorSet.h"

class DescriptorSet;
class Mesh;

struct Material
{
    // todo: find a better way?
    std::optional<DescriptorSet> TextureSet;
    Pipeline Pipeline;
};

class RenderObject
{
public:
    Mesh* Mesh{nullptr};
    Material* Material{nullptr};
    glm::mat4 Transform{};
};
