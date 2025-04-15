#include "SceneVisibility.h"

#include "FrameContext.h"
#include "Scene/SceneRenderObjectSet.h"
#include "cvars/CVarSystem.h"
#include "Rendering/Buffer/BufferUtility.h"
#include "Vulkan/Device.h"

void SceneVisibility::Init(const SceneVisibilityCreateInfo& createInfo, DeletionQueue& deletionQueue)
{
    m_Set = createInfo.Set;
    m_View = createInfo.View;

    m_RenderObjectVisibility = Device::CreateBuffer({
        .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.Visibility.Buffer.SizeBytes"_hsv),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
    m_MeshletVisibility = Device::CreateBuffer({
        .SizeBytes = (u64)*CVars::Get().GetI32CVar("Scene.Visibility.Meshlet.Buffer.SizeBytes"_hsv),
        .Usage = BufferUsage::Ordinary | BufferUsage::Storage | BufferUsage::Source},
        deletionQueue);
}

void SceneVisibility::OnUpdate(FrameContext& ctx)
{
    /* this buffer is managed entirely by gpu, here we only have to make sure that is has enough size */
    Buffers::grow(m_RenderObjectVisibility, m_Set->RenderObjectCount() / sizeof(SceneVisibilityBucket),
        ctx.CommandList);
    Buffers::grow(m_MeshletVisibility, m_Set->MeshletCount() / sizeof(SceneVisibilityBucket), ctx.CommandList);
}
