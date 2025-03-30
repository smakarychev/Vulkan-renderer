#pragma once

#include "Scene.h"
#include "ScenePass.h"

class SceneInfo;

class SceneRenderObjectSet
{
public:
    void Init(std::string_view name, Scene& scene, SceneBucketList& bucketList, Span<const ScenePassCreateInfo> passes,
        DeletionQueue& deletionQueue);

    void OnUpdate(FrameContext& ctx);

    u32 PassesCount() const { return (u32)m_Passes.size(); }
    u32 RenderObjectCount() const { return (u32)m_RenderObjectsCpu.size(); }
    SceneBucketHandle FirstBucket() const { return m_FirstBucket; }
    u32 BucketCount() const { return m_BucketCount; }
    u32 MeshletCount() const { return m_MeshletCount; }

    const std::vector<ScenePass>& Passes() const { return m_Passes; }
    
    Buffer BucketBits() const { return m_BucketBits.Buffer; }
    Buffer MeshletSpans() const { return m_MeshletSpans.Buffer; }
    
private:
    using InstanceData = Scene::NewInstanceData;
    void OnNewSceneInstance(const InstanceData& instanceData);
private:
    PushBufferTyped<SceneRenderObjectHandle> m_RenderObjects{};
    PushBufferTyped<SceneBucketBits> m_BucketBits{};
    PushBufferTyped<RenderObjectMeshletSpanGPU> m_MeshletSpans{};
    SceneBucketHandle m_FirstBucket{INVALID_SCENE_BUCKET};
    u32 m_BucketCount{0};
    u32 m_MeshletCount{0};

    SignalHandler<InstanceData> m_NewInstanceHandler;
    std::vector<SceneRenderObjectHandle> m_RenderObjectsCpu;
    std::vector<SceneBucketBits> m_BucketBitsCpu;
    std::vector<RenderObjectMeshletSpan> m_MeshletSpansCpu;
    std::vector<ScenePass> m_Passes;

    std::string m_Name{};
};
