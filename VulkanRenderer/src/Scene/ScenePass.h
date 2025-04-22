#pragma once

#include "SceneBucket.h"
#include "String/StringId.h"

struct ScenePassCreateInfo
{
    StringId Name{};
    std::vector<SceneBucketCreateInfo> BucketCreateInfos;
};

class ScenePass
{
    friend class SceneRenderObjectSet;
public:
    using FilterFn = SceneBucket::FilterFn;
    ScenePass(const ScenePassCreateInfo& createInfo, SceneBucketList& bucketList, DeletionQueue& deletionQueue);

    StringId Name() const { return m_Name; }
    
    void AddBuckets(Span<const SceneBucketCreateInfo> buckets, DeletionQueue& deletionQueue);
    SceneBucketHandle Filter(const SceneGeometryInfo& geometry, SceneRenderObjectHandle renderObject);
    void OnUpdate(FrameContext& ctx);

    u32 BucketCount() const { return (u32)m_BucketHandles.size(); }
    const std::vector<SceneBucketHandle>& BucketHandles() const { return m_BucketHandles; }
    const SceneBucket& BucketFromHandle(SceneBucketHandle handle) const { return m_BucketList->GetBucket(handle); }
    SceneBucket& BucketFromHandle(SceneBucketHandle handle) { return m_BucketList->GetBucket(handle); }

    const SceneBucket& FindBucket(StringId name) const;
    const SceneBucket* TryFindBucket(StringId name) const;
private:
    SceneBucketList* m_BucketList{nullptr};
    std::vector<SceneBucketHandle> m_BucketHandles;
    
    StringId m_Name{};
};
