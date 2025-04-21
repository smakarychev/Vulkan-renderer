#pragma once

#include "SceneVisibility.h"
#include "Rendering/Buffer/Buffer.h"
#include "Scene/SceneRenderObjectSet.h"

#include <array>

class SceneMultiviewVisibility
{
public:
    static constexpr u32 MAX_VIEWS = 64;
public:
    void Init(const SceneRenderObjectSet& set);
    void Shutdown();
    void OnUpdate(FrameContext& ctx);
    
    SceneVisibilityHandle AddVisibility(const SceneView& view);
    u32 VisibilityHandleToIndex(SceneVisibilityHandle handle) const { return handle.Handle; }

    Buffer RenderObjectVisibility(SceneVisibilityHandle handle) const;
    Buffer MeshletVisibility(SceneVisibilityHandle handle) const;
    const SceneView& View(SceneVisibilityHandle handle) const;
    
    u32 VisibilityCount() const { return m_ViewCount; }
    const SceneRenderObjectSet& ObjectSet() const;
private:
    void CreateVisibilityBuffers();
private:
    struct SceneVisibility
    {
        Buffer RenderObjectVisibility;
        Buffer MeshletVisibility;
        
        void OnUpdate(FrameContext& ctx, const SceneRenderObjectSet& set);
    };
    struct ViewVisibility
    {
        SceneVisibility Visibility{};
        SceneView View{};
    };
    std::array<ViewVisibility, MAX_VIEWS> m_Visibilities{};
    u32 m_ViewCountPreviousFrame{0};
    u32 m_ViewCount{0};
    const SceneRenderObjectSet* m_Set{nullptr};
};
