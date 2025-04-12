#pragma once

#include "Rendering/Buffer/Buffer.h"

#include <glm/vec2.hpp>

#include "Core/Camera.h"

struct FrameContext;
class DeletionQueue;
class SceneRenderObjectSet;

enum class SceneVisibilityFlags
{
    None = 0,
    TriangleCull    = BIT(1),
    ClampDepth      = BIT(2),
    OcclusionCull   = BIT(3),
    IsPrimaryView   = BIT(4),
};

CREATE_ENUM_FLAGS_OPERATORS(SceneVisibilityFlags)

struct SceneView
{
    Camera* Camera{nullptr};
    glm::uvec2 Resolution{};
    SceneVisibilityFlags VisibilityFlags{SceneVisibilityFlags::None};
};

struct SceneViewGPU
{
    static constexpr u32 IS_ORTHOGRAPHIC_BIT = 0;
    static constexpr u32 CLAMP_DEPTH_BIT = 1;
    
    glm::mat4 ViewMatrix{};
    glm::mat4 ViewProjectionMatrix{};
    FrustumPlanes FrustumPlanes{};
    ProjectionData ProjectionData{};
    glm::vec2 Resolution{};
    glm::vec2 HiZResolution{};

    u32 ViewFlags{0};

    static SceneViewGPU FromSceneView(const SceneView& view);
};

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