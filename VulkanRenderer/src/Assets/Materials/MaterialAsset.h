#pragma once
#include "Assets/AssetHandle.h"
#include "Assets/Images/ImageAsset.h"
#include "String/StringId.h"

namespace lux
{
enum class MaterialAlphaMode : u8
{
    Opaque, Mask, Translucent
};
struct MaterialAsset
{
    StringId Name;
    glm::vec4 BaseColor{1.0f};
    f32 Metallic{0.0f};
    f32 Roughness{1.0f};
    glm::vec3 EmissiveFactor{0.0f};
    
    MaterialAlphaMode AlphaMode{MaterialAlphaMode::Opaque};
    std::optional<f32> AlphaCutoff{std::nullopt};
    bool DoubleSided{false};
    std::optional<f32> OcclusionStrength{std::nullopt};

    ImageHandle BaseColorTexture{};
    ImageHandle EmissiveTexture{};
    ImageHandle NormalTexture{};
    ImageHandle MetallicRoughnessTexture{};
    ImageHandle OcclusionTexture{};
};
using MaterialHandle = AssetHandle<MaterialAsset>;
}
