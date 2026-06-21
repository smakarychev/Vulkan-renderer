#pragma once

#include "RenderObject.h"
#include "Assets/Materials/MaterialAsset.h"

#include <AssetLib/Scenes/Scene/SceneAsset.h>
#include <AssetLib/Scenes/Mesh/MeshAsset.h>
#include <CoreLib/Math/Transform.h>
#include <CoreLib/Containers/SlotMapType.h>

struct PointLight;
struct DirectionalLight;

namespace lux
{
struct SceneBlendShape;
struct SceneSkinJoints;
struct SceneHierarchyJoint;
struct SceneSkin;
struct SceneRenderObject;
struct CommonLight;
struct SceneHierarchyNode;
struct SceneHierarchyAnimation;
struct SceneHierarchyAnimationChannel;

struct SceneGeometryInfo
{
    std::vector<assetlib::SceneAssetIndexType> Indices;
    std::vector<glm::vec3> Positions;
    std::vector<glm::vec3> Normals;
    std::vector<glm::vec4> Tangents;
    std::vector<glm::vec2> UVs;
    std::vector<glm::u16vec4> Joints;
    std::vector<glm::vec4> Weights;

    std::vector<SceneRenderObject> RenderObjects;
    std::vector<SceneSkin> Skins;
    std::vector<SceneBlendShape> SceneBlendShapes;
    std::vector<SceneSkinJoints> SkinJoints;
    std::vector<MaterialGPU> Materials;
    std::vector<assetlib::SceneAssetMeshlet> Meshlets;
    std::vector<glm::mat4> JointInverseBindMatrices;
    
    struct MaterialInfo
    {
        MaterialHandle Handle{};
        u32 BaseColorUvIndex{0};
        u32 EmissiveUvIndex{0};
        u32 NormalUvIndex{0};
        u32 MetallicRoughnessUvIndex{0};
        u32 OcclusionUvIndex{0};
    };
    std::vector<MaterialInfo> MaterialsCpu;
};

struct SceneLightInfo
{
    std::vector<CommonLight> Lights;
    
    void SetSunLight(const DirectionalLight& light);
    void AddLight(const DirectionalLight& light);
    void AddLight(const PointLight& light);
};

struct SceneHierarchyInfo
{
    std::vector<SceneHierarchyNode> Nodes;
    std::vector<SceneHierarchyJoint> Joints;
    SlotMap<SceneHierarchyAnimationChannel> AnimationChannels;
    std::vector<SceneHierarchyAnimation> Animations;
    u16 MaxDepth{0};
};

struct SceneAsset
{
    SceneGeometryInfo Geometry{};
    SceneLightInfo Lights{};
    SceneHierarchyInfo Hierarchy{};
    
    void SetSunLight(const DirectionalLight& light);
    void AddLight(const DirectionalLight& light);
    void AddLight(const PointLight& light);
};

using SceneHandle = AssetHandle<SceneAsset>;
using SceneInstanceHandle = u32;

struct SceneRenderObject
{
    static constexpr u32 INVALID = ~0lu;
    
    u32 Material{};
    u32 FirstIndex{};
    u32 FirstVertex{};
    u32 VertexCount{};
    u32 FirstMeshlet{};
    u32 MeshletCount{};
    u32 SkinIndex{INVALID};
    u32 BlendShapeIndex{INVALID};
    u32 BlendShapeCount{};
    AABB BoundingBox{};
    Sphere BoundingSphere{};
};

struct SceneSkin
{
    u32 FirstJointMatrix{};
    u32 FirstJoint{};
    u32 FirstWeight{};
};

struct SceneBlendShape
{
    static constexpr u32 INVALID = ~0lu;
    
    std::string Name{};
    f32 Weight{};
    u32 FirstPosition{INVALID};
    u32 FirstNormal{INVALID};
    u32 FirstTangent{INVALID};
};

struct SceneSkinJoints
{
    std::vector<u32> JointNodes;
};

enum class LightType : u8
{
    Directional, Point, Spot
};

struct SpotLightData
{
    /* quantized */
    u16 InnerAngle{};
    u16 OuterAngle{};

    auto operator<=>(const SpotLightData&) const = default;
};

struct CommonLight
{
    LightType Type{LightType::Point};
    glm::vec3 PositionDirection{0.0f};
    glm::vec3 Color{1.0f};
    f32 Intensity{1.0f};
    f32 Radius{1.0f};
    SpotLightData SpotLightData{};
    bool IsSun{false};

    Transform3d GetTransform() const;
};

struct SceneHierarchyHandle
{
    static constexpr u32 INVALID = ~0lu;
    u32 Handle{INVALID};

    auto operator<=>(const SceneHierarchyHandle&) const = default;
    operator u32() const { return Handle; }
};

enum class SceneHierarchyNodeType : u16
{
    Dummy, Mesh, Light
};

struct SceneHierarchyNode
{
    SceneHierarchyNodeType Type{SceneHierarchyNodeType::Dummy};
    u16 Depth{0};
    SceneHierarchyHandle Parent{};
    Transform3d LocalTransform{};
    u32 PayloadIndex{0};
    SceneInstanceHandle Instance{};
};
struct SceneHierarchyJoint
{
    SceneHierarchyHandle Node{};
    u32 JointMatrixIndex{0};
    glm::mat4 InverseBindMatrix{};
};

enum class SceneHierarchyAnimationChannelType : u8
{
    Translation, Orientation, Scale, Weight
};
enum class SceneHierarchyAnimationSamplerType : u8
{
    Linear, Step, CubicSpline
};
struct SceneHierarchyAnimationChannel
{
    SceneHierarchyAnimationChannelType Type{SceneHierarchyAnimationChannelType::Translation};
    SceneHierarchyAnimationSamplerType SamplerType{SceneHierarchyAnimationSamplerType::Linear};

    union Keyframe
    {
        glm::vec3 Translation;
        glm::quat Orientation;
        glm::vec3 Scale;
        f32 Weight;
    };
    std::vector<Keyframe> Keyframes{};
    u32 KeyframeElementsCount{0};
    std::vector<f32> Timestamps{};
    Keyframe Interpolated{};
    f32 Timestamp{};
    u32 Frame{};
    
    void Tick(f32 dt);
};

struct SceneHierarchyAnimation
{
    static constexpr u32 INVALID = ~0lu;
    StringId Name{};
    SceneHierarchyHandle Node{};
    u32 TranslationChannel{INVALID};
    u32 OrientationChannel{INVALID};
    u32 ScaleChannel{INVALID};
    u32 WeightChannel{INVALID};
};
}
