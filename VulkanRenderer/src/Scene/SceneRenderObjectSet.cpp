#include "rendererpch.h"

#include "SceneRenderObjectSet.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "cvars/CVarSystem.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Vulkan/Device.h"

void SceneRenderObjectSet::Init(StringId name, Scene& scene, SceneBucketList& bucketList,
    Span<const ScenePassCreateInfo> passes, DeletionQueue& deletionQueue)
{
    m_Scene = &scene;
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
        .Description = {
            .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.RenderObjectSet.Buffer.SizeBytes"_hsv),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source
        },
    }, deletionQueue);
    
    m_Meshlets.Buffer = Device::CreateBuffer({
        .Description = {
            .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.RenderObjectSet.MeshletBuffer.SizeBytes"_hsv),
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
    const u32 newRenderObjects = (u32)m_RenderObjectsCpu.size() - m_RenderObjects.Offset;
    const u32 newMeshlets = (u32)m_MeshletsCpu.size() - m_Meshlets.Offset;
    PushBuffers::push<BufferAsymptoticGrowthPolicy>(m_RenderObjects,
        Span<SceneRenderObjectHandle>(m_RenderObjectsCpu).subspan(m_RenderObjects.Offset, newRenderObjects),
        ctx.CommandList, *ctx.ResourceUploader);
    PushBuffers::push<BufferAsymptoticGrowthPolicy>(m_Meshlets,
        Span<SceneMeshletHandle>(m_MeshletsCpu).subspan(m_Meshlets.Offset, newMeshlets),
        ctx.CommandList, *ctx.ResourceUploader);
    PushBuffers::push<BufferAsymptoticGrowthPolicy>(m_BucketBits,
        Span<SceneBucketBits>(m_BucketBitsCpu).subspan(m_BucketBits.Offset, newRenderObjects),
        ctx.CommandList, *ctx.ResourceUploader);
    for (auto& pass : m_Passes)
        pass.OnUpdate(ctx);
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
            const SceneRenderObjectHandle globalHandle = {
                .Index = handle.Index + instanceData.RenderObjectsOffset};
            m_RenderObjectsCpu.push_back(globalHandle);
            m_BucketBitsCpu.push_back(bucketBits);
            m_MeshletCount += renderObject.MeshletCount;
            for (u32 meshletIndex = 0; meshletIndex < renderObject.MeshletCount; meshletIndex++)
            {
                const SceneMeshletHandle globalMeshletHandle = {
                    .Index = renderObject.FirstMeshlet + meshletIndex + instanceData.MeshletsOffset};
                m_MeshletsCpu.push_back(globalMeshletHandle);
                m_TriangleCount += geometry.Meshlets[renderObject.FirstMeshlet + meshletIndex].IndexCount / 3;
            }
        }
    }
}

