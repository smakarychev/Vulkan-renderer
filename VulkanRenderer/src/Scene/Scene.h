#pragma once
#include "RenderObject.h"
#include "SceneAsset.h"
#include "SceneHierarchy.h"
#include "Rendering/Buffer/Buffer.h"
#include "Vulkan/Device.h"

namespace assetLib
{
    struct SceneInfo;
}

class BindlessTextureDescriptorsRingBuffer;

struct SceneMesh
{
    u32 Material{};
    BufferSubresource Indices{};
    BufferSubresource Positions{};
    BufferSubresource Normals{};
    BufferSubresource Tangents{};
    BufferSubresource UVs{};
    BufferSubresource Meshlets{};
    AABB BoundingBox{};
    Sphere BoundingSphere{};
};

/* todo: this has '2' in the name, should be removed once i get rid of old version */
struct SceneGeometry2
{
    BufferArena Indices{};
    BufferArena Attributes{};
    Buffer RenderObjects{};
    Buffer Meshlets{};
    Buffer Commands{};

    Buffer Materials{};

    u64 RenderObjectsOffsetBytes{0};
    u64 MeshletsOffsetBytes{0};
    u64 CommandsOffsetBytes{0};
    u64 MaterialsOffsetBytes{0};

    u32 CommandCount{0};
};

/* used to instantiate a scene */
class SceneInfo
{
    friend class Scene;
    friend class SceneHierarchy;
public:
    static SceneInfo* LoadFromAsset(std::string_view assetPath,
        BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue);
private:
    static void LoadBuffers(SceneInfo& scene, assetLib::SceneInfo& sceneInfo, DeletionQueue& deletionQueue);
    static void LoadMaterials(SceneInfo& scene, assetLib::SceneInfo& sceneInfo,
        BindlessTextureDescriptorsRingBuffer& texturesRingBuffer,DeletionQueue& deletionQueue);
    static void LoadMeshes(SceneInfo& scene, assetLib::SceneInfo& sceneInfo);
private:
    Buffer m_Buffer{};
    std::array<BufferSubresourceDescription, (u32)assetLib::SceneInfo::BufferViewType::MaxVal> m_Views;
    SceneHierarchyInfo m_Hierarchy{};
    std::vector<MaterialGPU> m_Materials{};
    std::vector<SceneMesh> m_Meshes{};
};

struct SceneInstantiationData
{
    Transform3d Transform{};
};

class Scene
{
public:
    static Scene CreateEmpty(DeletionQueue& deletionQueue);
    const SceneGeometry2& Geometry() const { return m_Geometry; }
    SceneGeometry2& Geometry() { return m_Geometry; }
    SceneHierarchy& Hierarchy() { return m_Hierarchy; }
    
    SceneInstance Instantiate(const SceneInfo& sceneInfo, const SceneInstantiationData& instantiationData,
        RenderCommandList& cmdList, ResourceUploader& uploader);
private:
    void InitGeometry(const SceneInfo& sceneInfo, RenderCommandList& cmdList, ResourceUploader& uploader);
    SceneInstance RegisterSceneInstance(const SceneInfo& sceneInfo);
private:
    struct SceneInfoGeometry
    {
        std::array<u32, (u32)assetLib::SceneInfo::BufferViewType::MaxVal> ElementOffsets;
    };
private:
    /* these values were revealed to me in a dream */
    static constexpr u64 DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES = 16llu * 1024 * 1024;
    static constexpr u64 DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_RENDER_OBJECTS_BUFFER_SIZE_BYTES = 1llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MESHLET_BUFFER_SIZE_BYTES = 2llu * 1024 * 1024;
    static constexpr u64 DEFAULT_COMMANDS_BUFFER_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MATERIALS_BUFFER_SIZE_BYTES = 1llu * 512 * 1024;

    SceneGeometry2 m_Geometry{};
    SceneHierarchy m_Hierarchy{};
    
    std::unordered_map<const SceneInfo*, u32> m_SceneInstancesMap{};
    std::unordered_map<const SceneInfo*, SceneInfoGeometry> m_SceneInfoGeometry{};
    u32 m_ActiveInstances{0};
};