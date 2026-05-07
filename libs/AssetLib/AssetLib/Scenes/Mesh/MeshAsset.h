#pragma once
#include <AssetLib/Io/AssetIo.h>
#include <AssetLib/AssetId.h>
#include <CoreLib/Math/Geometry.h>

#include <vector>

namespace lux::assetlib
{

static constexpr u32 MESH_UNSET_INDEX = ~0u; 

enum class MeshPrimitiveTextureFilter : u8
{
    Linear, Nearest
};
struct MeshPrimitiveTextureSample
{
    u32 UvIndex{MESH_UNSET_INDEX};
    MeshPrimitiveTextureFilter Filter{MeshPrimitiveTextureFilter::Linear};
};
struct MeshPrimitiveMaterial
{
    AssetId MaterialAsset{AssetId::CreateEmpty()};
    MeshPrimitiveTextureSample BaseColorSample{};
    MeshPrimitiveTextureSample EmissiveSample{};
    MeshPrimitiveTextureSample NormalSample{};
    MeshPrimitiveTextureSample MetallicRoughnessSample{};
    MeshPrimitiveTextureSample OcclusionSample{};
};

struct MeshPrimitive
{
    static constexpr std::string_view ATTRIBUTE_POSITION_NAME = "POSITION";
    static constexpr std::string_view ATTRIBUTE_NORMAL_NAME = "NORMAL";
    static constexpr std::string_view ATTRIBUTE_TANGENT_NAME = "TANGENT";
    static constexpr std::string_view ATTRIBUTE_UV0_NAME = "TEXCOORD_0";
    static constexpr std::string_view ATTRIBUTE_MESHLET_NAME = "MESHLET";
    struct Attribute
    {
        std::string Name;
        u32 Accessor{MESH_UNSET_INDEX};
    };
    std::vector<Attribute> Attributes;
    MeshPrimitiveMaterial Material{};
    u32 IndicesAccessor{MESH_UNSET_INDEX};
    Sphere BoundingSphere{};
    AABB BoundingBox{};
public:
    const Attribute* FindAttribute(std::string_view name) const;
};

struct MeshAsset
{
    AssetId GeometryBuffer{AssetId::CreateEmpty()};
    std::vector<MeshPrimitive> Primitives;
};

namespace sceneMesh
{
io::IoResult<MeshAsset> readMesh(const AssetMetadata& metadata);

io::IoResult<AssetPacked> pack(const MeshAsset& mesh);
}
}  
