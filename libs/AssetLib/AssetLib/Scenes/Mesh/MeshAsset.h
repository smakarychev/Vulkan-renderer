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

struct MeshAttribute
{
    static constexpr std::string_view POSITION_NAME = "POSITION";
    static constexpr std::string_view NORMAL_NAME = "NORMAL";
    static constexpr std::string_view TANGENT_NAME = "TANGENT";
    static constexpr std::string_view UV0_NAME = "TEXCOORD_0";
    static constexpr std::string_view JOINTS0_NAME = "JOINTS_0";
    static constexpr std::string_view WEIGHTS0_NAME = "WEIGHTS_0";
    static constexpr std::string_view MESHLET_NAME = "MESHLET";
    
    std::string Name{};
    u32 Accessor{MESH_UNSET_INDEX};
};

struct MeshPrimitiveBlendShape
{
    std::string Name{};
    std::vector<MeshAttribute> Attributes{};
    f32 Weight{0.0f};
public:
    const MeshAttribute* FindAttribute(std::string_view name) const;
};

struct MeshPrimitive
{
    std::vector<MeshAttribute> Attributes{};
    std::vector<MeshPrimitiveBlendShape> BlendShapes{};
    MeshPrimitiveMaterial Material{};
    u32 IndicesAccessor{MESH_UNSET_INDEX};
    Sphere BoundingSphere{};
    AABB BoundingBox{};
public:
    const MeshAttribute* FindAttribute(std::string_view name) const;
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
