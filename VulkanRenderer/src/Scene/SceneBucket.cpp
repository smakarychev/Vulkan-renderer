#include "rendererpch.h"

#include "SceneBucket.h"

#include "FrameContext.h"
#include "cvars/CVarSystem.h"
#include "Rendering/Buffer/BufferUtility.h"

SceneBucket::SceneBucket(const SceneBucketCreateInfo& createInfo, DeletionQueue& deletionQueue)
    : Filter(createInfo.Filter), ShaderOverrides(createInfo.ShaderOverrides), m_Name(createInfo.Name)
{
    m_Draws.Buffer = Device::CreateBuffer({
        .Description = {
            .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.Pass.DrawCommands.SizeBytes"_hsv),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Indirect | BufferUsage::Source
        },
    }, deletionQueue);
    m_DrawInfo = Device::CreateBuffer({
        .Description = {
            .SizeBytes = sizeof(SceneBucketDrawInfo),
            .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Uniform | BufferUsage::Indirect
        },
    }, deletionQueue);
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