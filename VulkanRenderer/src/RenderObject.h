#pragma once

#include <string>
#include <glm/glm.hpp>

#include "Settings.h"
#include "RenderHandle.h"

class DescriptorSet;
class Mesh;
class Image;

struct MaterialGPU
{
    static constexpr u32 NO_TEXTURE = std::numeric_limits<u32>::max();
    glm::vec4 Albedo;
    RenderHandle<Image> AlbedoTextureHandle{NO_TEXTURE};
    RenderHandle<Image> NormalTextureHandle{NO_TEXTURE};
    u32 Pad1{NO_TEXTURE};
    u32 Pad2{NO_TEXTURE};
};

struct RenderObject
{
    RenderHandle<Mesh> Mesh{};
    RenderHandle<MaterialGPU> MaterialGPU{};
    glm::mat4 Transform{};
};
