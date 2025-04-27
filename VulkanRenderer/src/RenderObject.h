#pragma once

#include "Settings.h"
#include "RenderHandle.h"
#include "SceneAsset.h"
#include "Math/Geometry.h"
#include "Math/Transform.h"
#include "Rendering/Image/Image.h"

enum class MaterialFlags : u16
{
    None = 0,
    Opaque      = BIT(1),
    AlphaMask   = BIT(2),
    Translucent = BIT(3),

    TwoSided    = BIT(4),
};
CREATE_ENUM_FLAGS_OPERATORS(MaterialFlags)

struct Material
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

struct RenderObjectGPU
{
    glm::mat4 Transform{glm::mat4{1.0}};
    Sphere BoundingSphere{};
    RenderHandle<MaterialGPU> MaterialGPU{};
    u32 PositionsOffset{0};
    u32 NormalsOffset{0};
    u32 TangentsOffset{0};
    u32 UVsOffset{0};
};

struct MeshletGPU
{
    assetLib::SceneInfo::BoundingCone BoundingCone;
    Sphere BoundingSphere;
};