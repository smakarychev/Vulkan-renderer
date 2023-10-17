#pragma once

#include <string>
#include <glm/glm.hpp>

#include "Settings.h"

class DescriptorSet;
class Mesh;

struct Material
{
    glm::vec4 Albedo;
    std::string AlbedoTexture{};
};

struct MaterialGPU
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
    MaterialGPU* MaterialBindless{nullptr};
    glm::mat4 Transform{};
};

struct BatchIndirect
{
    Mesh* Mesh{nullptr};
    MaterialGPU* MaterialBindless{nullptr};
    u32 First{0};
    u32 Count{0};
};