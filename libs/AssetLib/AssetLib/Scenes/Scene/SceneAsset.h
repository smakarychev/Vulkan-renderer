#pragma once
#include <AssetLib/Io/AssetIo.h>
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

struct SceneAssetSkin
{
    AssetId GeometryBuffer{AssetId::CreateEmpty()};
    u32 InverseBindMatrixAccessor{SCENE_UNSET_INDEX};
    std::vector<u32> JointNodes{};
};

enum class SceneAssetAnimationSamplerType : u8
{
    Linear, Step, CubicSpline
};

enum class SceneAssetAnimationChannelType : u8
{
    Translation, Orientation, Scale, Weight
};

struct SceneAssetAnimationChannel
{
    SceneAssetAnimationChannelType Type{};
    SceneAssetAnimationSamplerType SamplerType{};
    u32 TargetNode{SCENE_UNSET_INDEX};
    u32 TimestampsAccessor{SCENE_UNSET_INDEX};
    u32 KeyframesAccessor{SCENE_UNSET_INDEX};
    /* mainly used for blend shapes, where keyframe type is a float array */
    u32 KeyframeElementCount{1};
};

struct SceneAssetAnimation
{
    std::string Name;
    AssetId GeometryBuffer{AssetId::CreateEmpty()};
    std::vector<SceneAssetAnimationChannel> Channels;
};

struct SceneAssetNode
{
    std::string Name;
    std::vector<u32> Children{};
    u32 Camera{SCENE_UNSET_INDEX};
    u32 Light{SCENE_UNSET_INDEX};
    u32 Mesh{SCENE_UNSET_INDEX};
    u32 Skin{SCENE_UNSET_INDEX};
    Transform3d Transform{};
};

struct SceneAssetSubscene
{
    std::string Name;
    std::vector<u32> Nodes{};
};

using SceneAssetIndexType = u8;

struct SceneAsset
{
    static constexpr u32 TRIANGLES_PER_MESHLET = 256;
    static constexpr u32 VERTICES_PER_MESHLET = 128;
    
    std::vector<AssetId> Meshes;
    std::vector<SceneAssetSkin> Skins;
    std::vector<SceneAssetAnimation> Animations;
    std::vector<SceneAssetCamera> Cameras;
    std::vector<SceneAssetLight> Lights;
    std::vector<SceneAssetNode> Nodes;
    std::vector<SceneAssetSubscene> Subscenes;
    u32 DefaultSubscene{0};
};

namespace scene
{
io::IoResult<SceneAsset> readScene(const AssetMetadata& metadata);

io::IoResult<AssetPacked> pack(const SceneAsset& scene);
}

}