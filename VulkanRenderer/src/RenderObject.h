#pragma once

#include <glm/glm.hpp>

#include "ModelAsset.h"
#include "Settings.h"
#include "RenderHandle.h"

class DescriptorSet;
class Mesh;
class Image;

struct Material
{
    using MaterialType = assetLib::ModelInfo::MaterialType;
    using MaterialPropertiesPBR = assetLib::ModelInfo::MaterialPropertiesPBR;
    MaterialType Type;
    MaterialPropertiesPBR PropertiesPBR;
    std::vector<std::string> AlbedoTextures;
    std::vector<std::string> NormalTextures;
    std::vector<std::string> MetallicRoughnessTextures;
    std::vector<std::string> AmbientOcclusionTextures;   
};

struct MaterialGPU
{
    static constexpr u32 NO_TEXTURE = std::numeric_limits<u32>::max();
    glm::vec4 Albedo;
    f32 Metallic;
    f32 Roughness;
    f32 Pad0;
    f32 Pad1;
    RenderHandle<Image> AlbedoTextureHandle{NO_TEXTURE};
    RenderHandle<Image> NormalTextureHandle{NO_TEXTURE};
    RenderHandle<Image> MetallicRoughnessTextureHandle{NO_TEXTURE};
    RenderHandle<Image> AmbientOcclusionTextureHandle{NO_TEXTURE};
};

struct RenderObject
{
    RenderHandle<Mesh> Mesh{};
    RenderHandle<MaterialGPU> MaterialGPU{};
    RenderHandle<Material> Material{};
    glm::mat4 Transform{};
};

struct RenderObjectGPU
{
    glm::mat4 Transform;
    assetLib::BoundingSphere BoundingSphere;
};

struct MeshletGPU
{
    assetLib::BoundingCone BoundingCone;
    assetLib::BoundingSphere BoundingSphere;
};