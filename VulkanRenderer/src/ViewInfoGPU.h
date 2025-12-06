#pragma once

#include "core.h"
#include "types.h"

#include <glm/glm.hpp>

#include "RenderGraph/Passes/Generated/Types/AtmosphereSettingsUniform.generated.h"
#include "RenderGraph/Passes/Generated/Types/CameraUniform.generated.h"
#include "RenderGraph/Passes/Generated/Types/ShadingSettingsUniform.generated.h"
#include "RenderGraph/Passes/Generated/Types/ViewInfoUniform.generated.h"

class Camera;

/* todo: triangle culling ?
 * not a lot of possibilities are left for it to be useful:
 * - will not do occlusion culling, because storing persistent visibility
 *  for each triangle requires a little too much memory
 * - can not do backface culling, because different scene buckets require different winding
 * - frustum and screen-size culling may be more efficient with smaller meshlets
 */
enum class VisibilityFlags : u32
{
    None = 0,
    ClampDepth      = BIT(1),
    OcclusionCull   = BIT(2),
    IsPrimaryView   = BIT(3),
};

CREATE_ENUM_FLAGS_OPERATORS(VisibilityFlags)

struct CameraGPU : gen::Camera
{
    static constexpr u32 IS_ORTHOGRAPHIC_BIT = 0;
    static constexpr u32 CLAMP_DEPTH_BIT = 1;
    static CameraGPU FromCamera(const ::Camera& camera, const glm::uvec2& resolution, ::VisibilityFlags visibilityFlags
        = VisibilityFlags::None);
};

struct AtmosphereSettings : gen::AtmosphereSettings
{
    static AtmosphereSettings EarthDefault();
};

struct ShadingSettings : gen::ShadingSettings
{
    static ShadingSettings Default();
};

/*
 * Used for most of the 'pbr' passes of render graph
 */
struct ViewInfoGPU : gen::ViewInfo
{
    static ViewInfoGPU Default();
};