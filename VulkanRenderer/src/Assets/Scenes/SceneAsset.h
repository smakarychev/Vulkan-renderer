#pragma once

#include "RenderObject.h"
#include "Assets/Materials/MaterialAsset.h"

#include <AssetLib/Scenes/Scene/SceneAsset.h>
#include <AssetLib/Scenes/GeometryBuffer/GeometryBufferAsset.h>
#include <AssetLib/Scenes/Mesh/MeshAsset.h>
#include <CoreLib/Math/Transform.h>

struct PointLight;
struct DirectionalLight;

namespace lux
{
struct SceneSkin;
struct SceneHierarchyJoint;
struct SceneSkinnedRenderObject;
struct SceneRenderObject;
struct CommonLight;
struct SceneHierarchyNode;

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
    std::vector<SceneSkinnedRenderObject> SkinnedRenderObjects;
    std::vector<SceneSkin> Skins;
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
    
    void AddLight(const DirectionalLight& light);
    void AddLight(const PointLight& light);
};

struct SceneHierarchyInfo
{
    std::vector<SceneHierarchyNode> Nodes;
    std::vector<SceneHierarchyJoint> Joints;
    u16 MaxDepth{0};
};

struct SceneAsset
{
    SceneGeometryInfo Geometry{};
    SceneLightInfo Lights{};
    SceneHierarchyInfo Hierarchy{};
    
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
    u32 SkinnedRenderObjectIndex{INVALID};
    AABB BoundingBox{};
    Sphere BoundingSphere{};
};

struct SceneSkinnedRenderObject
{
    u32 FirstJointMatrix{};
    u32 FirstJoint{};
    u32 FirstWeight{};
    u32 SkinIndex{};
};

struct SceneSkin
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
    bool IsDeleted{false};

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
    glm::mat4 InverseBindMatrix{};
};
}
