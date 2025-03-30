#include "SceneRenderObjectSet.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "cvars/CVarSystem.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Vulkan/Device.h"

void SceneRenderObjectSet::Init(std::string_view name, Scene& scene, SceneBucketList& bucketList,
    Span<const ScenePassCreateInfo> passes, DeletionQueue& deletionQueue)
{
    m_FirstBucket = bucketList.Count();
    m_Name = name;
    m_Passes.reserve(passes.size());
    for (auto& pass : passes)
        m_Passes.emplace_back(pass, bucketList, deletionQueue);
    m_BucketCount = bucketList.Count() - m_FirstBucket;
    
    m_NewInstanceHandler = SignalHandler<InstanceData>([this](const InstanceData& instanceData)
    {
        OnNewSceneInstance(instanceData);
    });
    m_NewInstanceHandler.Connect(scene.GetInstanceAddedSignal());

    m_RenderObjects.Buffer = Device::CreateBuffer({
        .SizeBytes = (u64)*CVars::Get().GetI32CVar({"Scene.RenderObjectSet.Buffer.SizeBytes"}),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);

    m_BucketBits.Buffer = Device::CreateBuffer({
        .SizeBytes = (u64)*CVars::Get().GetI32CVar({"Scene.RenderObjectSet.RenderObjectBuckets.SizeBytes"}),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);

    m_MeshletSpans.Buffer = Device::CreateBuffer({
        .SizeBytes = (u64)*CVars::Get().GetI32CVar({"Scene.RenderObjectSet.MeshletSpan.SizeBytes"}),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
}

void SceneRenderObjectSet::OnUpdate(FrameContext& ctx)
{
    const u32 newRenderObjects = (u32)m_RenderObjectsCpu.size() - m_RenderObjects.Offset;
    PushBuffers::push<BufferAsymptoticGrowthPolicy>(m_RenderObjects,
        Span<SceneRenderObjectHandle>(m_RenderObjectsCpu).subspan(m_RenderObjects.Offset, newRenderObjects),
        ctx.CommandList, *ctx.ResourceUploader);
    PushBuffers::push<BufferAsymptoticGrowthPolicy>(m_BucketBits,
        Span<SceneBucketBits>(m_BucketBitsCpu).subspan(m_BucketBits.Offset, newRenderObjects),
        ctx.CommandList, *ctx.ResourceUploader);
    PushBuffers::push<BufferAsymptoticGrowthPolicy>(m_MeshletSpans,
        Span<RenderObjectMeshletSpanGPU>(m_MeshletSpansCpu).subspan(m_MeshletSpans.Offset, newRenderObjects),
        ctx.CommandList, *ctx.ResourceUploader);
    for (auto& pass : m_Passes)
        pass.OnUpdate(ctx);
}

void SceneRenderObjectSet::OnNewSceneInstance(const InstanceData& instanceData)
{
    const SceneGeometryInfo& geometry = instanceData.SceneInfo->m_Geometry;
    for (u32 renderObjectIndex = 0; renderObjectIndex < geometry.RenderObjects.size(); renderObjectIndex++)
    {
        const SceneRenderObjectHandle handle = {.Index = renderObjectIndex};
        SceneBucketBits bucketBits = {};
        for (auto& pass : m_Passes)
        {
            SceneBucketHandle bucket = pass.Filter(geometry, handle);
            if (bucket == INVALID_SCENE_BUCKET)
                continue;
            bucket = bucket - m_FirstBucket;
            ASSERT(bucket < MAX_BUCKETS_PER_SET)
            bucketBits |= 1llu << bucket;
        }

        if (bucketBits != 0)
        {
            auto& renderObject = geometry.RenderObjects[renderObjectIndex];
            m_RenderObjectsCpu.push_back(handle);
            m_BucketBitsCpu.push_back(bucketBits);
            m_MeshletSpansCpu.push_back({
                .Fist = renderObject.FirstMeshlet + instanceData.MeshletsOffset,
                .Count = renderObject.MeshletCount});
            m_MeshletCount += renderObject.MeshletCount;
        }
    }
}

