#include "ScenePass.h"

#include "FrameContext.h"
#include "ResourceUploader.h"
#include "cvars/CVarSystem.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Vulkan/Device.h"

SceneBucket::SceneBucket(const SceneBucketCreateInfo& createInfo, DeletionQueue& deletionQueue)
    : Filter(createInfo.Filter), m_Name(createInfo.Name)
{
    m_Draws.Buffer = Device::CreateBuffer({
        .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.Pass.DrawCommands.SizeBytes"_hsv),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Indirect | BufferUsage::Source},
        deletionQueue);
    m_DrawInfo = Device::CreateBuffer({
        .SizeBytes = sizeof(SceneBucketDrawInfo),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Uniform | BufferUsage::Indirect},
        deletionQueue);
}

void SceneBucket::OnUpdate(FrameContext& ctx)
{
    const u32 newDraws = m_DrawCount - m_Draws.Offset;
    /* this buffer is managed entirely by gpu, here we only have to make sure that is has enough size */
    PushBuffers::grow<BufferAsymptoticGrowthPolicy>(m_Draws, newDraws, ctx.CommandList);
    m_Draws.Offset = m_DrawCount;
}

void SceneBucket::AllocateRenderObjectDrawCommand(u32 meshletCount)
{
    m_DrawCount += meshletCount;
}

void SceneBucketList::Init(const Scene& scene)
{
    m_Scene = &scene;
}

SceneBucketHandle SceneBucketList::CreateBucket(const SceneBucketCreateInfo& createInfo, DeletionQueue& deletionQueue)
{
    const SceneBucketHandle id = (SceneBucketHandle)m_Buckets.size();
    auto& bucket = m_Buckets.emplace_back(createInfo, deletionQueue);
    bucket.m_Id = id;
    
    return id;
}

ScenePass::ScenePass(const ScenePassCreateInfo& createInfo, SceneBucketList& bucketList, DeletionQueue& deletionQueue)
    : m_BucketList(&bucketList), m_Name(createInfo.Name)
{
    AddBuckets(createInfo.BucketCreateInfos, deletionQueue);
}

void ScenePass::AddBuckets(Span<const SceneBucketCreateInfo> buckets, DeletionQueue& deletionQueue)
{
    for (auto& createInfo : buckets)
        m_BucketHandles.push_back(m_BucketList->CreateBucket(createInfo, deletionQueue));

    const SceneBucketHandle minHandle = std::ranges::min(m_BucketHandles);    
    const SceneBucketHandle maxHandle = std::ranges::max(m_BucketHandles);
    ASSERT(minHandle == m_BucketHandles.front(), "Something went really wrong")
    ASSERT(maxHandle - minHandle < MAX_BUCKETS_PER_SET,
        "Each scene pass must have no more than {} buckets, but got {}",
        MAX_BUCKETS_PER_SET, maxHandle - minHandle)
}

SceneBucketHandle ScenePass::Filter(const SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject)
{
    SceneBucketHandle bucketToAssignTo = INVALID_SCENE_BUCKET;
    for (auto bucketHandle : m_BucketHandles)
    {
        if (!m_BucketList->GetBucket(bucketHandle).Filter(geometry, renderObject))
            continue;
        ASSERT(bucketToAssignTo == INVALID_SCENE_BUCKET, "Ambiguous bucket")
        bucketToAssignTo = bucketHandle;
    }

    if (bucketToAssignTo != INVALID_SCENE_BUCKET)
        m_BucketList->GetBucket(bucketToAssignTo).AllocateRenderObjectDrawCommand(
            geometry.RenderObjects[renderObject.Index].MeshletCount);

    return bucketToAssignTo;
}

void ScenePass::OnUpdate(FrameContext& ctx)
{
    for (SceneBucketHandle bucketHandle : m_BucketHandles)
        m_BucketList->GetBucket(bucketHandle).OnUpdate(ctx);
}

const SceneBucket& ScenePass::FindBucket(StringId name) const
{
    const SceneBucket* bucket = TryFindBucket(name);
    ASSERT(bucket != nullptr, "Bucket with name {} not found", name);

    return *bucket;
}

const SceneBucket* ScenePass::TryFindBucket(StringId name) const
{
    for (auto& handle : m_BucketHandles)
    {
        auto& bucket = m_BucketList->GetBucket(handle);
        if (bucket.m_Name == name)
            return &bucket;
    }

    return nullptr;
}
