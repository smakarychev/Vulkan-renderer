#pragma once

#include "RenderHandle.h"
#include "SceneAsset.h"
#include "Math/Geometry.h"
#include "Math/Transform.h"
#include "RenderGraph/Passes/Generated/Types/RenderObjectUniform.generated.h"
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

struct RenderObjectGPU : gen::RenderObject {};

struct MeshletGPU
{
    assetLib::SceneInfo::BoundingCone BoundingCone;
    Sphere BoundingSphere;
};