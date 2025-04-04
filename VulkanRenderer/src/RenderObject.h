#pragma once

#include "ModelAsset.h"
#include "Settings.h"
#include "RenderHandle.h"
#include "Math/Geometry.h"
#include "Math/Transform.h"
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


enum class MaterialFlags : u16
{
    None = 0,
    Opaque      = BIT(1),
    AlphaMask   = BIT(2),
    Translucent = BIT(3),

    TwoSided    = BIT(4),
};
CREATE_ENUM_FLAGS_OPERATORS(MaterialFlags)

// todo: remove '2' once ready
struct Material2
{
    MaterialFlags Flags{MaterialFlags::None};
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
    glm::mat4 Transform{glm::mat4{1.0}};
    Sphere BoundingSphere{};
    RenderHandle<MaterialGPU> MaterialGPU{};
    u32 PositionsOffset{0};
    u32 NormalsOffset{0};
    u32 TangentsOffset{0};
    u32 UVsOffset{0};
};
struct RenderObjectMeshletSpan
{
    u32 Fist{0};
    u32 Count{0};
};
using RenderObjectMeshletSpanGPU = RenderObjectMeshletSpan;

struct MeshletGPU
{
    assetLib::BoundingCone BoundingCone;
    Sphere BoundingSphere;
};