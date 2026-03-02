#pragma once
#include <AssetLib/Io/AssetIo.h>
#include <CoreLib/core.h>
#include <CoreLib/Math/Geometry.h>
#include <CoreLib/Math/Transform.h>

#include <glm/glm.hpp>

namespace lux::assetlib
{
namespace io
{
class AssetCompressor;
class AssetIoInterface;
}

static constexpr u32 SCENE_UNSET_INDEX = ~0u; 

enum class SceneAssetAccessorComponentType : u8
{
    U8, U16, U32, F32, Meshlet
};
enum class SceneAssetAccessorType : u8
{
    Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4
};
struct SceneAssetAccessor
{
    u32 BufferView{SCENE_UNSET_INDEX};
    u64 OffsetBytes{0};
    SceneAssetAccessorComponentType ComponentType{SceneAssetAccessorComponentType::U8};
    u32 Count{0};
    SceneAssetAccessorType Type{SceneAssetAccessorType::Scalar};
    bool Normalize{false};
};

struct SceneAssetBuffer
{
    u64 SizeBytes{0};
};

enum class SceneAssetBufferViewType : u8
{
    /* vertex attributes */
    Position = 0,
    Normal = 1,
    Tangent = 2,
    Uv = 3,
            
    /* index */
    Index = 4,

    /* per instance data */
    Meshlet = 5,
            
    MaxVal = 6,
};
struct SceneAssetBufferView
{
    std::string Name;
    u32 Buffer{SCENE_UNSET_INDEX};
    u64 OffsetBytes{0};
    u64 LengthBytes{0};
};

struct SceneAssetPrimitive
{
    static constexpr std::string_view ATTRIBUTE_POSITION_NAME = "POSITION";
    static constexpr std::string_view ATTRIBUTE_NORMAL_NAME = "NORMAL";
    static constexpr std::string_view ATTRIBUTE_TANGENT_NAME = "TANGENT";
    static constexpr std::string_view ATTRIBUTE_UV0_NAME = "TEXCOORD_0";
    static constexpr std::string_view ATTRIBUTE_MESHLET_NAME = "MESHLET";
    struct Attribute
    {
        std::string Name;
        u32 Accessor{SCENE_UNSET_INDEX};
    };
    std::vector<Attribute> Attributes;
    u32 Material{SCENE_UNSET_INDEX};
    u32 IndicesAccessor{SCENE_UNSET_INDEX};
    Sphere BoundingSphere{};
    AABB BoundingBox{};

public:
    Attribute* FindAttribute(std::string_view name);
};
struct SceneAssetMesh
{
    std::vector<SceneAssetPrimitive> Primitives;
};

enum class SceneAssetTextureFilter : u8
{
    Linear, Nearest
};
struct SceneAssetTextureSample
{
    u32 UvIndex{SCENE_UNSET_INDEX};
    SceneAssetTextureFilter Filter{SceneAssetTextureFilter::Linear};
};
struct SceneAssetMaterial
{
    std::string Name;
    AssetId MaterialAsset{AssetId::CreateEmpty()};
    SceneAssetTextureSample BaseColorSample{};
    SceneAssetTextureSample EmissiveSample{};
    SceneAssetTextureSample NormalSample{};
    SceneAssetTextureSample MetallicRoughnessSample{};
    SceneAssetTextureSample OcclusionSample{};
};

enum class SceneAssetCameraType : u8
{
    Perspective, Orthographic
};
struct SceneAssetCamera
{
    struct PerspectiveData
    {
        f32 Aspect{16.0f / 9.0f};
        f32 FovY{glm::radians(60.0f)};
    };
    struct OrthographicData
    {
        f32 SpanX{1000.0f};
        f32 SpanY{1000.0f};
    };
    SceneAssetCameraType Type{SceneAssetCameraType::Perspective};
    f32 Near{0.1f};
    f32 Far{std::numeric_limits<f32>::infinity()};
    std::optional<PerspectiveData> Perspective{std::nullopt};
    std::optional<OrthographicData> Orthographic{std::nullopt};
};

enum class SceneAssetLightType : u8
{
    Directional, Point, Spot
};
struct SceneAssetLight
{
    SceneAssetLightType Type{SceneAssetLightType::Point};
    glm::vec3 Color{1.0f};
    f32 Intensity{1.0f};
    f32 Range{1.0f};
    // todo: spotlight
};

struct SceneAssetMeshlet
{
    struct BoundingCone
    {
        i8 AxisX;
        i8 AxisY;
        i8 AxisZ;
        i8 Cutoff;
    };
    
    u32 FirstIndex{};
    u32 IndexCount{};

    u32 FirstVertex{};
    u32 VertexCount{};

    Sphere Sphere{};
    BoundingCone Cone{};
};

struct SceneAssetNode
{
    std::string Name;
    std::vector<u32> Children{};
    u32 Camera{SCENE_UNSET_INDEX};
    u32 Light{SCENE_UNSET_INDEX};
    u32 Mesh{SCENE_UNSET_INDEX};
    Transform3d Transform{};
};

struct SceneAssetSubscene
{
    std::string Name;
    std::vector<u32> Nodes{};
};


struct SceneAssetHeader
{
    std::vector<SceneAssetAccessor> Accessors;
    std::vector<SceneAssetBuffer> Buffers;
    std::vector<SceneAssetBufferView> BufferViews;
    std::vector<SceneAssetMesh> Meshes;
    std::vector<SceneAssetMaterial> Materials;
    std::vector<SceneAssetCamera> Cameras;
    std::vector<SceneAssetLight> Lights;
    std::vector<SceneAssetNode> Nodes;
    std::vector<SceneAssetSubscene> Subscenes;
    u32 DefaultSubscene{0};
};

using SceneAssetIndexType = u8;

struct SceneAsset
{
    static constexpr u32 TRIANGLES_PER_MESHLET = 256;
    static constexpr u32 VERTICES_PER_MESHLET = 255;
    
    SceneAssetHeader Header{};
    std::vector<std::vector<std::byte>> BuffersData{};
};

namespace scene
{
io::IoResult<SceneAssetHeader> readHeader(const AssetFile& assetFile);
io::IoResult<std::vector<std::byte>> readBufferData(const SceneAssetHeader& header, const AssetFile& assetFile,
    u32 bufferIndex, io::AssetIoInterface& io, io::AssetCompressor& compressor);
io::IoResult<SceneAsset> readScene(const AssetFile& assetFile,
    io::AssetIoInterface& io, io::AssetCompressor& compressor);

io::IoResult<AssetPacked> pack(const SceneAsset& scene, io::AssetCompressor& compressor);

AssetMetadata getMetadata();
}

}