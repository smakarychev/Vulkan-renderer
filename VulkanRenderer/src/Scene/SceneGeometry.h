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
    struct AddRenderObjectsResult
    {
        u32 FirstRenderObject{0};
    };
public:
    static SceneGeometry CreateEmpty(DeletionQueue& deletionQueue);
    void Add(const lux::SceneAsset& scene, FrameContext& ctx);
    AddRenderObjectsResult AddRenderObjects(const lux::SceneAsset& scene, lux::SceneInstanceHandle instance,
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
            
        /* index */
        Index = 4,

        /* per instance data */
        MeshletBounds = 5,
        Meshlets = 6,
        
        Materials = 7,
            
        MaxVal = 8,
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
    BufferArena Materials{};

    RenderHandleArray<lux::MaterialHandle> MaterialsCpu;
private:
    std::unordered_map<const lux::SceneAsset*, SceneInfoOffsets> m_SceneInfoOffsets{};

    struct SceneInstanceInfo
    {
        BufferSuballocation RenderObjectsSuballocation{};
    };
    std::unordered_map<u32, SceneInstanceInfo> m_InstancesInfo;

    // todo: to cvars? (upd: yes, please do)
    /* these values were revealed to me in a dream */
    static constexpr u64 DEFAULT_ARENA_VIRTUAL_SIZE_BYTES = 16llu * 1024 * 1024 * 1024;
    static constexpr u64 DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES = 16llu * 1024 * 1024;
    static constexpr u64 DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MESHLETS_BUFFER_ARENA_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_RENDER_OBJECTS_BUFFER_ARENA_SIZE_BYTES = 1llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MATERIALS_BUFFER_ARENA_SIZE_BYTES = 1llu * 512 * 1024;
};