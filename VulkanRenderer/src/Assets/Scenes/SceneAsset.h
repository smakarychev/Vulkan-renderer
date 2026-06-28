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
    std::vector<SceneBlendShape> BlendShapes;
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
    u32 FirstPosition{};
    u32 FirstNormal{};
    u32 FirstTangent{};
    u32 FirstUv{};
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
    static constexpr u32 INVALID = ~0lu;
    
    u32 FirstJointMatrix{INVALID};
    u32 FirstJoint{INVALID};
    u32 FirstWeight{INVALID};
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
    static constexpr u32 INVALID = ~0u;
    SceneHierarchyNodeType Type{SceneHierarchyNodeType::Dummy};
    u16 Depth{0};
    SceneHierarchyHandle Parent{};
    Transform3d LocalTransform{};
    u32 PayloadIndex{0};
    u32 BlendShapeBaseIndex{INVALID};
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
    union Keyframe
    {
        glm::vec3 Translation;
        glm::quat Orientation;
        glm::vec3 Scale;
        f32 Weight;
    };
 
    SceneHierarchyAnimationChannel(
        SceneHierarchyAnimationChannelType type, SceneHierarchyAnimationSamplerType samplerType, u32 elementCount);
    void Tick(f32 dt);
    const Keyframe& GetInterpolated() const;
    const Keyframe& GetInterpolated(u32 elementIndex) const;
    
    std::vector<Keyframe>& KeyframesMutable() { return m_Keyframes; }
    std::vector<f32>& TimestampsMutable() { return m_Timestamps; }
    u32 ElementCount() const { return m_KeyframeElementCount; }
private:
    void Interpolate();
    void InterpolateTranslation(f32 t);
    void InterpolateOrientation(f32 t);
    void InterpolateScale(f32 t);
    void InterpolateWeight(f32 t);
    void UpdateTimestamp(f32 dt);
    void UpdateTimestampPositive();
    void UpdateTimestampNegative();
    const Keyframe& GetKeyframe() const;
    const Keyframe& GetKeyframe(u32 elementIndex) const;
    const Keyframe& GetNextKeyframe() const;
    const Keyframe& GetNextKeyframe(u32 elementIndex) const;
private:
    SceneHierarchyAnimationChannelType m_Type{SceneHierarchyAnimationChannelType::Translation};
    SceneHierarchyAnimationSamplerType m_SamplerType{SceneHierarchyAnimationSamplerType::Linear};
    u32 m_KeyframeElementCount{0};
    
    std::vector<Keyframe> m_InterpolatedArray{};
    f32 m_Timestamp{};
    u32 m_Frame{};
    
    std::vector<Keyframe> m_Keyframes{};
    std::vector<f32> m_Timestamps{};
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
