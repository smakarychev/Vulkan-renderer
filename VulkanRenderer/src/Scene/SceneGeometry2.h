#pragma once

#include "Rendering/Buffer/BufferArena.h"

#include <glm/glm.hpp>

#include "RenderObject.h"
#include "SceneAsset.h"
#include "SceneInstance.h"

struct FrameContext;
class BindlessTextureDescriptorsRingBuffer;

struct SceneMesh
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

    std::vector<SceneMesh> Meshes;
    std::vector<MaterialGPU> Materials;
    std::vector<assetLib::ModelInfo::Meshlet> Meshlets;
};

/* todo: this has '2' in the name, should be removed once i get rid of old version */
class SceneGeometry2
{
public:
    static SceneGeometry2 CreateEmpty(DeletionQueue& deletionQueue);
    void Add(SceneInstance instance, FrameContext& ctx);
    void AddCommands(SceneInstance instance, FrameContext& ctx);
public:
    struct UgbOffsets
    {
        std::array<u32, (u32)assetLib::SceneInfo::BufferViewType::MaxVal> ElementOffsets;
    };
public:
    BufferArena Indices{};
    BufferArena Attributes{};
    Buffer RenderObjects{};
    Buffer Meshlets{};
    Buffer Commands{};
    Buffer Materials{};
    u32 CommandCount{0};
private:
    u64 m_RenderObjectsOffsetBytes{0};
    u64 m_MeshletsOffsetBytes{0};
    u64 m_CommandsOffsetBytes{0};
    u64 m_MaterialsOffsetBytes{0};
    
    std::unordered_map<const SceneInfo*, UgbOffsets> m_UgbOffsets{};

    
    /* these values were revealed to me in a dream */
    static constexpr u64 DEFAULT_ATTRIBUTES_BUFFER_ARENA_SIZE_BYTES = 16llu * 1024 * 1024;
    static constexpr u64 DEFAULT_INDICES_BUFFER_ARENA_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_RENDER_OBJECTS_BUFFER_SIZE_BYTES = 1llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MESHLET_BUFFER_SIZE_BYTES = 2llu * 1024 * 1024;
    static constexpr u64 DEFAULT_COMMANDS_BUFFER_SIZE_BYTES = 4llu * 1024 * 1024;
    static constexpr u64 DEFAULT_MATERIALS_BUFFER_SIZE_BYTES = 1llu * 512 * 1024;
};