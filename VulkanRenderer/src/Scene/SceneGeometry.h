#pragma once

#include "Rendering/Buffer/BufferArena.h"

#include "RenderHandleArray.h"
#include "RenderObject.h"
#include "SceneInfo.h"
#include "SceneInstance.h"
#include "Rendering/Buffer/PushBuffer.h"

#include <AssetLib/Scenes/SceneAsset.h>
#include <CoreLib/Math/Geometry.h>

#include <glm/glm.hpp>

namespace lux
{
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
    void Add(SceneInstance instance, FrameContext& ctx);
    AddRenderObjectsResult AddRenderObjects(SceneInstance instance, FrameContext& ctx);

    void SetScene(Scene& scene) { m_Scene = &scene; }
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
            
        MaxVal = 7,
    };
    struct SceneInfoOffsets
    {
        std::array<u32, (u32)SceneInfoOffsetType::MaxVal> ElementOffsets;
        u32 MaterialOffset{0};
    };
public:
    BufferArena Indices{};
    BufferArena Attributes{};
    BufferArena Meshlets{};
    PushBuffer RenderObjects{};
    PushBuffer Materials{};

    RenderHandleArray<Material> MaterialsCpu;
private:
    std::unordered_map<const SceneInfo*, SceneInfoOffsets> m_SceneInfoOffsets{};
    Scene* m_Scene{nullptr};

    // todo: to cvars? (upd: yes, please do)
    /* these values were revealed to me in a dream */
    static constexpr u64 DEFAULT_ARENA_VIRTUAL_SIZE_BYTES = 16llu * 1024 * 1024 * 1024;
    static constexpr u64 DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES = 16llu * 1024 * 1024;
    static constexpr u64 DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MESHLETS_BUFFER_ARENA_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_RENDER_OBJECTS_BUFFER_SIZE_BYTES = 1llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MATERIALS_BUFFER_SIZE_BYTES = 1llu * 512 * 1024;
};