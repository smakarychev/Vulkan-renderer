#pragma once

#include <string>
#include <glm/glm.hpp>

#include "Settings.h"
#include "RenderHandle.h"

class DescriptorSet;
class Mesh;
class Image;

struct Material
{
    glm::vec4 Albedo;
    std::string AlbedoTexture{};
};

struct MaterialGPU
{
    static constexpr u32 NO_TEXTURE = std::numeric_limits<u32>::max();
    glm::vec4 Albedo;
    RenderHandle<Image> AlbedoTextureHandle{NO_TEXTURE};
    u32 Pad0{NO_TEXTURE};
    u32 Pad1{NO_TEXTURE};
    u32 Pad2{NO_TEXTURE};
};

struct RenderObject
{
    RenderHandle<Mesh> Mesh{};
    RenderHandle<MaterialGPU> MaterialGPU{};
    glm::mat4 Transform{};
};

struct BatchIndirect
{
    RenderHandle<Mesh> Mesh{};
    RenderHandle<MaterialGPU> MaterialGPU{};
    u32 First{0};
    u32 InstanceCount{0};
};