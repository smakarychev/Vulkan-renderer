#include "SceneMultiviewVisibility.h"

#include "FrameContext.h"
#include "cvars/CVarSystem.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Scene/SceneRenderObjectSet.h"
#include "Vulkan/Device.h"

void SceneMultiviewVisibility::SceneVisibility::OnUpdate(FrameContext& ctx, const SceneRenderObjectSet& set)
{
    /* this buffer is managed entirely by gpu, here we only have to make sure that is has enough size */
    Buffers::grow(RenderObjectVisibility, set.RenderObjectCount() / sizeof(SceneVisibilityBucket),
        ctx.CommandList);
    Buffers::grow(MeshletVisibility, set.MeshletCount() / sizeof(SceneVisibilityBucket), ctx.CommandList);
}

void SceneMultiviewVisibility::Init(const SceneRenderObjectSet& set)
{
    m_Set = &set;
}

void SceneMultiviewVisibility::OnUpdate(FrameContext& ctx)
{
    m_ViewCountPreviousFrame = m_ViewCount;
    m_ViewCount = 0;
    for (u32 i = 0; i < m_ViewCountPreviousFrame; i++)
        m_Visibilities[i].Visibility.OnUpdate(ctx, *m_Set);
}

SceneVisibilityHandle SceneMultiviewVisibility::AddVisibility(const SceneView& view, DeletionQueue& deletionQueue)
{
    if (m_ViewCount >= MAX_VIEWS)
    {
        LOG("Multiview visibility cannot take more than {} views", MAX_VIEWS);
        return {SceneVisibilityHandle::INVALID};
    }

    m_Visibilities[m_ViewCount].View = view;
    
    if (m_ViewCount >= m_ViewCountPreviousFrame)
        CreateVisibilityBuffers(deletionQueue);
    
    const SceneVisibilityHandle toReturn {.Handle = m_ViewCount};
    m_ViewCount++;

    return toReturn;
}

Buffer SceneMultiviewVisibility::RenderObjectVisibility(SceneVisibilityHandle handle) const
{
    ASSERT(handle <= m_ViewCount, "Invalid visibility handle")
    
    return m_Visibilities[handle].Visibility.RenderObjectVisibility;
}

Buffer SceneMultiviewVisibility::MeshletVisibility(SceneVisibilityHandle handle) const
{
    ASSERT(handle <= m_ViewCount, "Invalid visibility handle")

    return m_Visibilities[handle].Visibility.MeshletVisibility;
}

const SceneView& SceneMultiviewVisibility::View(SceneVisibilityHandle handle) const
{
    ASSERT(handle <= m_ViewCount, "Invalid visibility handle")

    return m_Visibilities[handle].View;
}

const SceneRenderObjectSet& SceneMultiviewVisibility::ObjectSet() const
{
    return *m_Set;
}

void SceneMultiviewVisibility::CreateVisibilityBuffers(DeletionQueue& deletionQueue)
{
    m_Visibilities[m_ViewCount].Visibility.RenderObjectVisibility = Device::CreateBuffer({
        .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.Visibility.Buffer.SizeBytes"_hsv),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
    m_Visibilities[m_ViewCount].Visibility.MeshletVisibility = Device::CreateBuffer({
        .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.Visibility.Meshlet.Buffer.SizeBytes"_hsv),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
}
