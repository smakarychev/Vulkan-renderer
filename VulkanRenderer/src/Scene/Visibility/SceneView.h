#pragma once

#include "core.h"
#include "Core/Camera.h"

/* todo: triangle culling ?
 * not a lot of possibilities are left for it to be useful:
 * - will not do occlusion culling, because storing persistent visibility
 *  for each triangle requires a little too much memory
 * - can not do backface culling, because different scene buckets require different winding
 * - frustum and screen-size culling may be more efficient with smaller meshlets
 */
enum class SceneVisibilityFlags
{
    None = 0,
    ClampDepth      = BIT(1),
    OcclusionCull   = BIT(2),
    IsPrimaryView   = BIT(3),
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