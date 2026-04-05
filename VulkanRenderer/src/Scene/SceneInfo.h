#pragma once

#include "Rendering/Buffer/BufferArena.h"

#include "RenderHandleArray.h"
#include "RenderObject.h"
#include "SceneInstance.h"

#include <AssetLib/Scenes/SceneAsset.h>

struct DirectionalLight;
struct PointLight;
class BindlessTextureDescriptorsRingBuffer;

namespace lux
{
class ImageAssetManager;
class AssetSystem;
class MaterialAssetManager;
}

struct SceneRenderObject
{
    u32 Material{};
    u32 FirstIndex{};
    u32 FirstVertex{};
    u32 FirstMeshlet{};
    u32 MeshletCount{};
    AABB BoundingBox{};
    Sphere BoundingSphere{};
};

struct SceneGeometryInfo
{
    static SceneGeometryInfo FromAsset(lux::assetlib::SceneAsset& scene,
        BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue,
        lux::AssetSystem& assetSystem,
        lux::ImageAssetManager& imageAssetManager,
        lux::MaterialAssetManager& materialAssetManager);
    
    std::vector<lux::assetlib::SceneAssetIndexType> Indices;
    std::vector<glm::vec3> Positions;
    std::vector<glm::vec3> Normals;
    std::vector<glm::vec4> Tangents;
    std::vector<glm::vec2> UVs;

    std::vector<SceneRenderObject> RenderObjects;
    std::vector<MaterialGPU> Materials;
    std::vector<lux::assetlib::SceneAssetMeshlet> Meshlets;
    
    std::vector<Material> MaterialsCpu;
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
    SceneInstance Instance{};
};

struct SceneHierarchyInfo
{
    static SceneHierarchyInfo FromAsset(const lux::assetlib::SceneAsset& scene);
    
    std::vector<SceneHierarchyNode> Nodes;
    u16 MaxDepth{0};
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

struct SceneLightInfo
{
    static SceneLightInfo FromAsset(lux::assetlib::SceneAsset& scene);
    void AddLight(const DirectionalLight& light);
    void AddLight(const PointLight& light);
    
    std::vector<CommonLight> Lights;
};

/* used to instantiate a scene */
class SceneInfo
{
    friend class Scene;
    friend class SceneGeometry;
    friend class SceneLight;
    friend class SceneRenderObjectSet;
public:
    static SceneInfo* LoadFromAsset(std::string_view assetPath,
        BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue,
    lux::AssetSystem& assetSystem,
    lux::ImageAssetManager& imageAssetManager,
    lux::MaterialAssetManager& materialAssetManager);
    
    void AddLight(const DirectionalLight& light);
    void AddLight(const PointLight& light);
private:
    SceneGeometryInfo m_Geometry{};
    SceneLightInfo m_Lights{};
    SceneHierarchyInfo m_Hierarchy{};
};
