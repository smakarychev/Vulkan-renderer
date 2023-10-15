#pragma once

#include <glm/glm.hpp>

#include "Settings.h"
#include "Vulkan/Shader.h"

class DescriptorSet;
class Mesh;

struct Material
{
    glm::vec4 Albedo;
    std::string AlbedoTexture{};
};

struct MaterialBindless
{
    static constexpr u32 NO_TEXTURE = std::numeric_limits<u32>::max();
    glm::vec4 Albedo;
    u32 AlbedoTextureIndex{NO_TEXTURE};
    u32 Pad0{NO_TEXTURE};
    u32 Pad1{NO_TEXTURE};
    u32 Pad2{NO_TEXTURE};
};

class RenderObject
{
public:
    Mesh* Mesh{nullptr};
    Material* Material{nullptr};
    MaterialBindless* MaterialBindless{nullptr};
    glm::mat4 Transform{};
};

struct BatchIndirect
{
    Mesh* Mesh{nullptr};
    MaterialBindless* MaterialBindless{nullptr};
    u32 First{0};
    u32 Count{0};
};