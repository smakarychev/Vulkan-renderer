#pragma once

#include "SceneGeometry2.h"
#include "Rendering/Commands/RenderCommands.h"
#include "Rendering/Shader/ShaderOverrides.h"
#include "String/StringId.h"

struct SceneRenderObjectHandle
{
    u32 Index{0};
};

struct SceneMeshletHandle
{
    u32 Index{0};
};

using SceneBucketBits = u64;
static constexpr u32 MAX_BUCKETS_PER_SET = std::numeric_limits<SceneBucketBits>::digits;

struct SceneMeshletBucketInfo
{
    u32 MeshletIndex{0};
    SceneBucketBits Buckets{0};    
};

/* pass over filtered render objects of a scene */

using SceneBucketHandle = u32;
static constexpr SceneBucketHandle INVALID_SCENE_BUCKET{~0lu};

struct SceneBucketCreateInfo
{
    using FilterFn = std::function<bool(const SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject)>;
    StringId Name{};
    FilterFn Filter{};
    ShaderOverrides ShaderOverrides{};
};

struct SceneBucketDrawInfo
{
    u32 Count{0};
};

class SceneBucket
{
    friend class SceneBucketList;
    friend class ScenePass;
public:
    SceneBucket(const SceneBucketCreateInfo& createInfo, DeletionQueue& deletionQueue);
    
    SceneBucketHandle Handle() const { return m_Id; }
    StringId Name() const { return m_Name; }

    Buffer Draws() const { return m_Draws.Buffer; }
    Buffer DrawInfo() const { return m_DrawInfo; }
public:
    using FilterFn = SceneBucketCreateInfo::FilterFn;
    FilterFn Filter{};
    mutable ShaderOverrides ShaderOverrides{};
private:
    void OnUpdate(FrameContext& ctx);
    void AllocateRenderObjectDrawCommand(u32 meshletCount);
private:
    SceneBucketHandle m_Id{~0lu};

    u32 m_DrawCount{0};
    PushBufferTyped<IndirectDrawCommand> m_Draws{};
    Buffer m_DrawInfo{};
    
    StringId m_Name{};
};

class SceneBucketList
{
public:
    using FilterFn = SceneBucket::FilterFn;
    void Init(const Scene& scene);

    SceneBucketHandle CreateBucket(const SceneBucketCreateInfo&  createInfo, DeletionQueue& deletionQueue);

    const SceneBucket& GetBucket(SceneBucketHandle handle) const { return m_Buckets[handle]; }
    SceneBucket& GetBucket(SceneBucketHandle handle) { return m_Buckets[handle]; }
    
    u32 Count() const { return (u32)m_Buckets.size(); }
private:
    std::vector<SceneBucket> m_Buckets;
    const Scene* m_Scene{nullptr};
};