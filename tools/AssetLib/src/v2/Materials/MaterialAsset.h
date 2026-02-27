#pragma once
#include "types.h"
#include "v2/AssetId.h"
#include "v2/Io/AssetIo.h"

#include <optional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace lux::assetlib
{
namespace io
{
class AssetCompressor;
class AssetIoInterface;
}

enum class MaterialAlphaMode : u8
{
    Opaque, Mask, Translucent
};

struct MaterialAsset
{
    std::string Name;
    glm::vec4 BaseColor{1.0f};
    f32 Metallic{0.0f};
    f32 Roughness{1.0f};
    glm::vec3 EmissiveFactor{0.0f};
    
    MaterialAlphaMode AlphaMode{MaterialAlphaMode::Opaque};
    std::optional<f32> AlphaCutoff{std::nullopt};
    bool DoubleSided{false};
    std::optional<f32> OcclusionStrength{std::nullopt};

    AssetId BaseColorTexture{AssetId::CreateEmpty()};
    AssetId EmissiveTexture{AssetId::CreateEmpty()};
    AssetId NormalTexture{AssetId::CreateEmpty()};
    AssetId MetallicRoughnessTexture{AssetId::CreateEmpty()};
    AssetId OcclusionTexture{AssetId::CreateEmpty()};
};

namespace material
{
io::IoResult<MaterialAsset> readMaterial(const AssetFile& assetFile);

io::IoResult<AssetPacked> pack(const AssetFile& material);

AssetMetadata getMetadata();
}
}
