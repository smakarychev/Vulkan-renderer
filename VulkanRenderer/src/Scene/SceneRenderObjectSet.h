#pragma once

#include "Scene.h"
#include "ScenePass.h"
#include "String/StringId.h"

class SceneInfo;

class SceneRenderObjectSet
{
public:
    void Init(StringId name, Scene& scene, SceneBucketList& bucketList, Span<const ScenePassCreateInfo> passes,
        DeletionQueue& deletionQueue);

    void OnUpdate(FrameContext& ctx);

    u32 PassesCount() const { return (u32)m_Passes.size(); }
    u32 RenderObjectCount() const { return (u32)m_RenderObjectsCpu.size(); }
    SceneBucketHandle FirstBucket() const { return m_FirstBucket; }
    u32 BucketHandleToIndex(SceneBucketHandle bucket) const { return bucket - m_FirstBucket; }
    u32 BucketCount() const { return m_BucketCount; }
    u32 MeshletCount() const { return m_MeshletCount; }
    u32 TriangleCount() const { return m_TriangleCount; }

    const std::vector<ScenePass>& Passes() const { return m_Passes; }
    
    Buffer BucketBits() const { return m_BucketBits.Buffer; }
    Buffer RenderObjectHandles() const { return m_RenderObjects.Buffer; }
    Buffer MeshletHandles() const { return m_Meshlets.Buffer; }

    const SceneGeometry2& Geometry() const { return m_Scene->Geometry(); }

    const ScenePass& FindPass(StringId name) const;
    const ScenePass* TryFindPass(StringId name) const;
    
private:
    using InstanceData = Scene::NewInstanceData;
    void OnNewSceneInstance(const InstanceData& instanceData);
private:
    PushBufferTyped<SceneRenderObjectHandle> m_RenderObjects{};
    PushBufferTyped<SceneMeshletHandle> m_Meshlets{};
    PushBufferTyped<SceneBucketBits> m_BucketBits{};
    SceneBucketHandle m_FirstBucket{INVALID_SCENE_BUCKET};
    u32 m_BucketCount{0};
    u32 m_MeshletCount{0};
    u32 m_TriangleCount{0};

    const Scene* m_Scene{nullptr};

    SignalHandler<InstanceData> m_NewInstanceHandler;
    std::vector<SceneRenderObjectHandle> m_RenderObjectsCpu;
    std::vector<SceneMeshletHandle> m_MeshletsCpu;
    std::vector<SceneBucketBits> m_BucketBitsCpu;
    std::vector<ScenePass> m_Passes;

    StringId m_Name{};
};
