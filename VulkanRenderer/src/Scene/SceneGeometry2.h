#pragma once

#include "Rendering/Buffer/BufferArena.h"

#include <glm/glm.hpp>

#include "RenderHandleArray.h"
#include "RenderObject.h"
#include "SceneAsset.h"
#include "SceneInstance.h"
#include "Math/Geometry.h"
#include "Rendering/Buffer/PushBuffer.h"

struct FrameContext;
class BindlessTextureDescriptorsRingBuffer;

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
    static SceneGeometryInfo FromAsset(assetLib::SceneInfo& sceneInfo,
        BindlessTextureDescriptorsRingBuffer& texturesRingBuffer, DeletionQueue& deletionQueue);
    
    std::vector<assetLib::ModelInfo::IndexType> Indices;
    std::vector<glm::vec3> Positions;
    std::vector<glm::vec3> Normals;
    std::vector<glm::vec4> Tangents;
    std::vector<glm::vec2> UVs;

    std::vector<SceneRenderObject> RenderObjects;
    std::vector<MaterialGPU> Materials;
    std::vector<assetLib::ModelInfo::Meshlet> Meshlets;
    
    std::vector<Material2> MaterialsCpu;
};

/* todo: this has '2' in the name, should be removed once i get rid of old version */
class SceneGeometry2
{
public:
    struct AddCommandsResult
    {
        u32 FirstRenderObject{0};
        u32 FirstMeshlet{0};
    };
public:
    static SceneGeometry2 CreateEmpty(DeletionQueue& deletionQueue);
    void Add(SceneInstance instance, FrameContext& ctx);
    AddCommandsResult AddCommands(SceneInstance instance, FrameContext& ctx);
public:
    struct SceneInfoOffsets
    {
        std::array<u32, (u32)assetLib::SceneInfo::BufferViewType::MaxVal> ElementOffsets;
        u32 MaterialOffset{0};
    };
public:
    BufferArena Indices{};
    BufferArena Attributes{};
    PushBuffer RenderObjects{};
    PushBuffer Meshlets{};
    PushBuffer Commands{};
    PushBuffer Materials{};
    u32 CommandCount{0};

    RenderHandleArray<Material2> MaterialsCpu;
private:
    std::unordered_map<const SceneInfo*, SceneInfoOffsets> m_SceneInfoOffsets{};

    // todo: to cvars? (upd: yes, please do)
    /* these values were revealed to me in a dream */
    static constexpr u64 DEFAULT_ARENA_VIRTUAL_SIZE_BYTES = 16llu * 1024 * 1024 * 1024;
    static constexpr u64 DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES = 16llu * 1024 * 1024;
    static constexpr u64 DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_RENDER_OBJECTS_BUFFER_SIZE_BYTES = 1llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MESHLET_BUFFER_SIZE_BYTES = 2llu * 1024 * 1024;
    static constexpr u64 DEFAULT_COMMANDS_BUFFER_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MATERIALS_BUFFER_SIZE_BYTES = 1llu * 512 * 1024;
};