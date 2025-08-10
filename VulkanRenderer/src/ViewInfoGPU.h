#pragma once

#include "types.h"

#include <glm/glm.hpp>

#include "RenderHandle.h"
#include "Core/Camera.h"
#include "Rendering/Image/Image.h"

class Camera;

/* todo: triangle culling ?
 * not a lot of possibilities are left for it to be useful:
 * - will not do occlusion culling, because storing persistent visibility
 *  for each triangle requires a little too much memory
 * - can not do backface culling, because different scene buckets require different winding
 * - frustum and screen-size culling may be more efficient with smaller meshlets
 */
enum class VisibilityFlags
{
    None = 0,
    ClampDepth      = BIT(1),
    OcclusionCull   = BIT(2),
    IsPrimaryView   = BIT(3),
};

CREATE_ENUM_FLAGS_OPERATORS(VisibilityFlags)

struct CameraGPU
{
    static constexpr u32 IS_ORTHOGRAPHIC_BIT = 0;
    static constexpr u32 CLAMP_DEPTH_BIT = 1;

    glm::mat4 ViewProjection{glm::mat4{1.0f}};
    glm::mat4 Projection{glm::mat4{1.0f}};
    glm::mat4 View{glm::mat4{1.0f}};

    glm::vec3 Position{};
    f32 Near{0.1f};
    glm::vec3 Forward{glm::vec3{0.0f, 0.0f, 1.0f}};
    f32 Far{1000.0f};

    glm::mat4 InverseViewProjection{glm::mat4{1.0f}};
    glm::mat4 InverseProjection{glm::mat4{1.0f}};
    glm::mat4 InverseView{glm::mat4{1.0f}};

    FrustumPlanes FrustumPlanes{};
    ProjectionData ProjectionData{};

    glm::vec2 Resolution{1.0f};
    glm::vec2 HiZResolution{};

    u32 ViewFlagsGpu{0};
    VisibilityFlags VisibilityFlags{VisibilityFlags::None};

    static CameraGPU FromCamera(const Camera& camera, const glm::uvec2& resolution, ::VisibilityFlags visibilityFlags
        = VisibilityFlags::None);
};

struct AtmosphereSettings
{
    glm::vec4 RayleighScattering{};
    glm::vec4 RayleighAbsorption{};
    glm::vec4 MieScattering{};
    glm::vec4 MieAbsorption{};
    glm::vec4 OzoneAbsorption{};
    glm::vec4 SurfaceAlbedo{};
    
    f32 Surface{};
    f32 Atmosphere{};
    f32 RayleighDensity{};
    f32 MieDensity{};
    f32 OzoneDensity{};

    static AtmosphereSettings EarthDefault();
};

struct ShadingSettings
{
    f32 EnvironmentPower{1.0f};
    u32 SoftShadows{false};
    f32 MaxLightCullDistance{1.0f};
    f32 VolumetricCloudShadowStrength{0.3f};
    RenderHandle<Image> TransmittanceLut{};
    RenderHandle<Image> SkyViewLut{};
    RenderHandle<Image> VolumetricCloudShadow{};

    u32 Padding[1]{};

    glm::mat4 VolumetricCloudViewProjection{};
    glm::mat4 VolumetricCloudView{};
};

/*
 * Used for most of the 'pbr' passes of render graph
 * Reflected in `view.glsl`
 */
struct ViewInfoGPU
{
    CameraGPU Camera{};
    CameraGPU PreviousCamera{};
    AtmosphereSettings Atmosphere{};
    ShadingSettings ShadingSettings{};
    f32 FrameNumber{0.0f};
    u32 FrameNumberU32{0};
};