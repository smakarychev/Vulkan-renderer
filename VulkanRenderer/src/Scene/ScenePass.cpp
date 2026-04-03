#include "rendererpch.h"

#include "ScenePass.h"

#include "ResourceUploader.h"
#include "SceneInfo.h"

ScenePass::ScenePass(const ScenePassCreateInfo& createInfo, SceneBucketList& bucketList)
    : m_BucketList(&bucketList), m_Name(createInfo.Name)
{
    AddBuckets(createInfo.BucketCreateInfos);
}

void ScenePass::AddBuckets(Span<const SceneBucketCreateInfo> buckets)
{
    for (auto& createInfo : buckets)
        m_BucketHandles.push_back(m_BucketList->CreateBucket(createInfo));

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

    return bucketToAssignTo;
}

const SceneBucket& ScenePass::FindBucket(StringId name) const
{
    const SceneBucket* bucket = TryFindBucket(name);
    ASSERT(bucket != nullptr, "Bucket with name {} not found", name)

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
