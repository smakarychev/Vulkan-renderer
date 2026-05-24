#pragma once

#include "Rendering/Buffer/BufferArena.h"

#include "RenderHandleArray.h"
#include "RenderObject.h"
#include "SceneInstance.h"
#include "Assets/Materials/MaterialAsset.h"

namespace lux
{
struct SceneAsset;
class ImageAssetManager;
class AssetSystem;
}

namespace lux
{
class MaterialAssetManager;
}

struct FrameContext;
class BindlessTextureDescriptorsRingBuffer;

class SceneGeometry
{
public:
    struct AddInstanceResult
    {
        u32 FirstRenderObject{0};
        u32 FirstJoint{0};
        u32 FirstJointMatrix{0};
    };
public:
    static SceneGeometry CreateEmpty(DeletionQueue& deletionQueue);
    void Add(const lux::SceneAsset& scene, FrameContext& ctx);
    AddInstanceResult AddInstance(const lux::SceneAsset& scene, lux::SceneInstanceHandle instance,
        FrameContext& ctx);
    void UpdateMaterials(const lux::SceneAsset& scene, FrameContext& ctx);
    void Delete(const lux::SceneAsset& scene);
    void DeleteRenderObjects(lux::SceneInstanceHandle instance);
    
public:
    enum class SceneInfoOffsetType : u8
    {
        /* vertex attributes */
        Position = 0,
        Normal = 1,
        Tangent = 2,
        Uv = 3,
        Joint = 4,
        Weight = 5,
            
        /* index */
        Index = 6,

        /* per instance data */
        MeshletBounds = 7,
        Meshlets = 8,
        
        Materials = 9,
            
        MaxVal = 10,
    };
    struct SceneInfoOffsets
    {
        std::array<u32, (u32)SceneInfoOffsetType::MaxVal> ElementOffsets;
        std::array<BufferSuballocationHandle, (u32)SceneInfoOffsetType::MaxVal> Suballocations;
    };
public:
    BufferArena Indices{};
    BufferArena Attributes{};
    BufferArena Meshlets{};
    BufferArena RenderObjects{};
    BufferArena JointMatrices{};
    BufferArena Skins{};
    BufferArena Materials{};
    BufferArena RenderObjectSkinnedInfos{};
    std::vector<u32> RenderObjectSkinnedInfosIndices{};

    RenderHandleArray<lux::MaterialHandle> MaterialsCpu;
    
    u32 SkinnedRenderObjectCount{0};
    u32 SkinnedMeshletCount{0};
    u32 SkinnedVertexCount{0};
private:
    std::unordered_map<const lux::SceneAsset*, SceneInfoOffsets> m_SceneInfoOffsets{};

    struct SceneInstanceInfo
    {
        BufferSuballocation RenderObjectsSuballocation{};
        BufferSuballocation RenderObjectSkinnedInfosSuballocation{};
        BufferSuballocation JointMatricesSuballocation{};
        BufferSuballocation SkinsSuballocation{};
        BufferSuballocation SkinnedVertexSuballocation{};
        BufferSuballocation SkinnedMeshletBoundSuballocation{};
        u32 SkinnedRenderObjectCount{};
        u32 SkinnedMeshletCount{};
        u32 SkinnedVertexCount{};
        u32 FirstRenderObjectSkinnedInfoIndex{};
    };
    std::unordered_map<u32, SceneInstanceInfo> m_InstancesInfo;

    // todo: to cvars? (upd: yes, please do)
    /* these values were revealed to me in a dream */
    static constexpr u64 DEFAULT_ARENA_VIRTUAL_SIZE_BYTES = 16llu * 1024 * 1024 * 1024;
    static constexpr u64 DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES = 16llu * 1024 * 1024;
    static constexpr u64 DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MESHLETS_BUFFER_ARENA_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_RENDER_OBJECTS_BUFFER_ARENA_SIZE_BYTES = 1llu * 1024 * 1024;
    static constexpr u64 DEFAULT_SKINNED_RENDER_OBJECTS_BUFFER_ARENA_SIZE_BYTES = 1llu * 512 * 1024;
    static constexpr u64 DEFAULT_JOINT_MATRICES_BUFFER_ARENA_SIZE_BYTES = 1llu * 1024 * 1024;
    static constexpr u64 DEFAULT_SKINS_BUFFER_ARENA_SIZE_BYTES = 1llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MATERIALS_BUFFER_ARENA_SIZE_BYTES = 1llu * 512 * 1024;
};