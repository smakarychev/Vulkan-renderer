#pragma once

#include "Rendering/Buffer/Buffer.h"

#include "SceneView.h"

class ScenePass;
struct FrameContext;
class DeletionQueue;
class SceneRenderObjectSet;

struct SceneVisibilityCreateInfo
{
    const SceneRenderObjectSet* Set{nullptr};
    SceneView View{};
};

using SceneVisibilityBucket = u64;

class SceneVisibility
{
    friend class SceneMultiviewVisibility;
public:
    void Init(const SceneVisibilityCreateInfo& createInfo, DeletionQueue& deletionQueue);
    void OnUpdate(FrameContext& ctx);

    Buffer RenderObjectVisibility() const { return m_RenderObjectVisibility; }
    Buffer MeshletVisibility() const { return m_MeshletVisibility; }
    
    SceneViewGPU GetViewGPU() const { return SceneViewGPU::FromSceneView(m_View); }
    const SceneView& GetView() const { return m_View; }
private:
    Buffer m_RenderObjectVisibility;
    Buffer m_MeshletVisibility;
    const SceneRenderObjectSet* m_Set{nullptr};
    SceneView m_View{};
};