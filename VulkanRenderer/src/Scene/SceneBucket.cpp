#include "rendererpch.h"

#include "SceneBucket.h"

SceneBucket::SceneBucket(const SceneBucketCreateInfo& createInfo)
    : Filter(createInfo.Filter), ShaderOverrides(createInfo.ShaderOverrides), m_Name(createInfo.Name)
{
}

void SceneBucketList::Init(const Scene& scene)
{
    m_Scene = &scene;
}

SceneBucketHandle SceneBucketList::CreateBucket(const SceneBucketCreateInfo& createInfo)
{
    const SceneBucketHandle id = (SceneBucketHandle)m_Buckets.size();
    auto& bucket = m_Buckets.emplace_back(createInfo);
    bucket.m_Id = id;
    
    return id;
}