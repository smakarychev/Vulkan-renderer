#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "ModelAsset.h"
#include "Settings.h"
#include "RenderHandle.h"
#include "Common/Geometry.h"
#include "Common/Transform.h"
#include "Rendering/Image/Image.h"

class Mesh;

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
    std::vector<std::string> EmissiveTextures;   
};

struct MaterialGPU
{
    static constexpr u32 NO_TEXTURE = std::numeric_limits<u32>::max();
    glm::vec4 Albedo;
    f32 Metallic;
    f32 Roughness;
    f32 Pad0{};
    RenderHandle<Image> AlbedoTextureHandle{NO_TEXTURE};
    RenderHandle<Image> NormalTextureHandle{NO_TEXTURE};
    RenderHandle<Image> MetallicRoughnessTextureHandle{NO_TEXTURE};
    RenderHandle<Image> AmbientOcclusionTextureHandle{NO_TEXTURE};
    RenderHandle<Image> EmissiveTextureHandle{NO_TEXTURE};
};

using RenderObjectTransform = Transform3d;

struct RenderObject
{
    RenderHandle<Mesh> Mesh{};
    RenderHandle<MaterialGPU> MaterialGPU{};
    RenderHandle<Material> Material{};
    RenderObjectTransform Transform{};
};

struct RenderObjectGPU
{
    glm::mat4 Transform;
    Sphere BoundingSphere;
};
// todo: remove '2' once ready
struct RenderObjectGPU2
{
    glm::mat4 Transform;
    Sphere BoundingSphere;
    RenderHandle<MaterialGPU> MaterialGPU{};
    u32 PositionsOffset{0};
    u32 NormalsOffset{0};
    u32 TangentsOffset{0};
    u32 UVsOffset{0};
};

struct MeshletGPU
{
    assetLib::BoundingCone BoundingCone;
    Sphere BoundingSphere;
};