#pragma once

#include "SceneVisibility.h"
#include "Scene/SceneRenderObjectSet.h"

#include <array>

class SceneMultiviewVisibility
{
public:
    static constexpr u32 MAX_VIEWS = 32;
public:
    void Init(const SceneRenderObjectSet& set);
    void OnUpdate(FrameContext& ctx);
    
    SceneVisibilityHandle AddVisibility(const SceneView& view);
    u32 VisibilityHandleToIndex(SceneVisibilityHandle handle) const { return handle.Handle; }

    const SceneView& View(SceneVisibilityHandle handle) const;
    
    u32 VisibilityCount() const { return m_ViewCount; }
    const SceneRenderObjectSet& ObjectSet() const;
private:
    std::array<SceneView, MAX_VIEWS> m_Visibilities{};
    u32 m_ViewCount{0};
    const SceneRenderObjectSet* m_Set{nullptr};
};
