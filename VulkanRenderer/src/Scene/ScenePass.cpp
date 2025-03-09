#include "ScenePass.h"

SceneBucket::SceneBucket(std::string_view name, FilterFn filter)
    : Filter(std::move(filter)), m_Name(name)
{
}

ScenePass::ScenePass(std::string_view name, Span<const SceneBucket> buckets)
    : m_Name(name)
{
    m_Buckets.append_range(buckets);
    m_RenderObjectsBucketIndex.resize(buckets.size());
}

bool ScenePass::Filter(const SceneGeometryInfo& geometry, u32 renderObjectIndex)
{
    static constexpr u32 INVALID_BUCKET = ~0lu;
    u32 bucketToAssignTo = INVALID_BUCKET;
    for (u32 bucketIndex = 0; bucketIndex < m_Buckets.size(); bucketIndex++)
    {
        if (m_Buckets[bucketIndex].Filter(geometry, renderObjectIndex))
        {
            ASSERT(bucketToAssignTo == INVALID_BUCKET, "Ambiguous bucket")
            bucketToAssignTo = bucketIndex;
        }
    }

    if (bucketToAssignTo != INVALID_BUCKET)
        m_RenderObjectsBucketIndex.push_back(bucketToAssignTo);

    return bucketToAssignTo != INVALID_BUCKET;
}
