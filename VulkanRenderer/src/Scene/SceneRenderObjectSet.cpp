#include "rendererpch.h"

#include "SceneRenderObjectSet.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "cvars/CVarSystem.h"
#include "Rendering/Buffer/BufferUtility.h"

void SceneRenderObjectSet::Init(StringId name, Scene& scene, SceneBucketList& bucketList,
    Span<const ScenePassCreateInfo> passes, DeletionQueue& deletionQueue)
{
    m_Scene = &scene;
    m_FirstBucket = bucketList.Count();
    m_Name = name;
    m_Passes.reserve(passes.size());
    for (auto& pass : passes)
        m_Passes.emplace_back(pass, bucketList);
    m_BucketCount = bucketList.Count() - m_FirstBucket;
    
    m_NewInstanceHandler = SignalHandler<NewInstanceData>([this](const NewInstanceData& instanceData) {
        OnNewSceneInstance(instanceData);
    });
    m_DeletedInstanceHandler = SignalHandler<DeletedInstanceData>([this](const DeletedInstanceData& instanceData) {
        OnDeletedSceneInstance(instanceData);
    });
    m_NewInstanceHandler.Connect(scene.GetInstanceAddedSignal());
    m_DeletedInstanceHandler.Connect(scene.GetInstanceDeletedSignal());

    m_RenderObjects.Buffer = Device::CreateBuffer({
        .Description = {
            .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.RenderObjectSet.Buffer.SizeBytes"_hsv),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
        },
    }, deletionQueue);

    m_BucketBits.Buffer = Device::CreateBuffer({
        .Description = {
            .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.RenderObjectSet.RenderObjectBuckets.SizeBytes"_hsv),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
        },
    }, deletionQueue);
}

void SceneRenderObjectSet::OnUpdate(FrameContext& ctx)
{
    const i32 newRenderObjects = (i32)m_RenderObjectsCpu.size() - (i32)m_RenderObjects.Offset;
    if (newRenderObjects > 0)
    {
        PushBuffers::grow<BufferAsymptoticGrowthPolicy>(m_RenderObjects, newRenderObjects, ctx.CommandList);
        PushBuffers::grow<BufferAsymptoticGrowthPolicy>(m_BucketBits, newRenderObjects, ctx.CommandList);
    }
    // todo: we don't need to upload the entire buffer each frame
    ctx.ResourceUploader->UpdateBuffer(m_RenderObjects.Buffer, m_RenderObjectsCpu);
    ctx.ResourceUploader->UpdateBuffer(m_BucketBits.Buffer, m_BucketBitsCpu);
}

const ScenePass& SceneRenderObjectSet::FindPass(StringId name) const
{
    const ScenePass* pass = TryFindPass(name);
    ASSERT(pass != nullptr, "Pass with name {} not found", name)

    return *pass;
}

const ScenePass* SceneRenderObjectSet::TryFindPass(StringId name) const
{
    for (auto& pass : m_Passes)
        if (pass.m_Name == name)
            return &pass;
    
    return nullptr;
}

void SceneRenderObjectSet::OnNewSceneInstance(const NewInstanceData& instanceData)
{
    const SceneGeometryInfo& geometry = instanceData.Instance.m_SceneInfo->m_Geometry;

    static constexpr u32 INVALID_ID = ~0u;
    SceneInstanceInfo addedInstanceInfo = {.FirstRenderObject = INVALID_ID};
    
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
            const SceneRenderObjectHandle globalHandle = {.Index = handle.Index + instanceData.RenderObjectsOffset};
            u32 renderObjectCpuIndex = (u32)m_RenderObjectsCpu.size();
            m_RenderObjectsCpu.push_back(globalHandle);
            m_BucketBitsCpu.push_back(bucketBits);
            m_MeshletCount += renderObject.MeshletCount;
            u32 trianglesCount = 0;
            for (u32 meshletIndex = 0; meshletIndex < renderObject.MeshletCount; meshletIndex++)
                trianglesCount += geometry.Meshlets[renderObject.FirstMeshlet + meshletIndex].IndexCount / 3;
            m_TriangleCount += trianglesCount;
            
            if (addedInstanceInfo.FirstRenderObject == INVALID_ID)
                addedInstanceInfo.FirstRenderObject = renderObjectCpuIndex;
            addedInstanceInfo.RenderObjectCount += 1;
            addedInstanceInfo.MeshletCount += renderObject.MeshletCount;
            addedInstanceInfo.TriangleCount += trianglesCount;
        }
    }

    m_InstancesInfo.emplace(instanceData.Instance.m_InstanceId, addedInstanceInfo);
}

void SceneRenderObjectSet::OnDeletedSceneInstance(const DeletedInstanceData& instanceData)
{
    const SceneInstanceInfo& sceneInstance = m_InstancesInfo.at(instanceData.Instance.m_InstanceId);

    m_RenderObjectsCpu.erase(
        m_RenderObjectsCpu.begin() + sceneInstance.FirstRenderObject,
        m_RenderObjectsCpu.begin() + sceneInstance.FirstRenderObject + sceneInstance.RenderObjectCount);
    m_BucketBitsCpu.erase(
        m_BucketBitsCpu.begin() + sceneInstance.FirstRenderObject,
        m_BucketBitsCpu.begin() + sceneInstance.FirstRenderObject + sceneInstance.RenderObjectCount);
    m_MeshletCount -= sceneInstance.MeshletCount;
    m_TriangleCount -= sceneInstance.TriangleCount;

    for (auto& instanceInfo : m_InstancesInfo | std::views::values)
        if (instanceInfo.FirstRenderObject > sceneInstance.FirstRenderObject)
            instanceInfo.FirstRenderObject -= sceneInstance.RenderObjectCount;
    
    m_InstancesInfo.erase(instanceData.Instance.m_InstanceId);
}
