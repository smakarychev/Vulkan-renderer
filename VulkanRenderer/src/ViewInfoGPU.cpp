#include "rendererpch.h"

#include "ViewInfoGPU.h"

#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "Rendering/Image/ImageUtility.h"

CameraGPU CameraGPU::FromCamera(const ::Camera& camera, const glm::uvec2& resolution, ::VisibilityFlags visibilityFlags)
{
    u32 viewFlags = {};
    viewFlags |= (u32)(camera.GetType() == CameraType::Orthographic) << IS_ORTHOGRAPHIC_BIT;
    viewFlags |= (u32)enumHasAny(visibilityFlags, VisibilityFlags::ClampDepth) << CLAMP_DEPTH_BIT;
    const glm::uvec2 hizResolution = enumHasAny(visibilityFlags, VisibilityFlags::OcclusionCull) ?
        Images::floorResolutionToPowerOfTwo(resolution) : glm::uvec2(0);
    const f32 maxCullDistance = *CVars::Get().GetF32CVar("Renderer.Limits.MaxGeometryCullDistance"_hsv);

    const auto& frustumPlanes = camera.GetFrustumPlanes(maxCullDistance);
    const auto& projectionData = camera.GetProjectionData();
    
    CameraGPU cameraGPU = {{
        .ViewProjection = camera.GetViewProjection(),
        .Projection = camera.GetProjection(),
        .View = camera.GetView(),
        .Position = camera.GetPosition(),
        .Near = camera.GetNear(),
        .Forward = camera.GetForward(),
        .Far = camera.GetFar(),
        .Right = camera.GetRight(),
        .Fov = camera.GetFov(),
        .Up = camera.GetUp(),
        .AspectRatio = camera.GetAspect(),
        .InverseViewProjection = glm::inverse(camera.GetViewProjection()),
        .InverseProjection = glm::inverse(camera.GetProjection()),
        .InverseView = glm::inverse(camera.GetView()),
        .FrustumTopY = frustumPlanes.TopY,
        .FrustumTopZ = frustumPlanes.TopZ,
        .FrustumRightX = frustumPlanes.RightX,
        .FrustumRightZ = frustumPlanes.RightZ,
        .FrustumNear = frustumPlanes.Near,
        .FrustumFar = frustumPlanes.Far,
        .ProjectionWidth = projectionData.Width,
        .ProjectionHeight = projectionData.Height,
        .ProjectionBiasX = projectionData.BiasX,
        .ProjectionBiasY = projectionData.BiasY,
        .Resolution = glm::vec2{resolution},
        .HizResolution = hizResolution,
        .ViewFlags = viewFlags,
        .VisibilityFlags = (u32)visibilityFlags,
    }};

    return cameraGPU;
}

AtmosphereSettings AtmosphereSettings::EarthDefault()
{
    return {{
        .RayleighScattering = glm::vec4{0.005802f, 0.013558f, 0.0331f, 1.0f},
        .RayleighAbsorption = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f},
        .MieScattering = glm::vec4{0.003996f, 0.003996f, 0.003996f, 1.0f},
        .MieAbsorption = glm::vec4{0.0044f, 0.0044f, 0.0044f, 1.0f},
        .OzoneAbsorption = glm::vec4{0.000650f, 0.001881f, 0.000085f, 1.0f},
        .SurfaceAlbedo = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f},
        .Surface = 6360.0f,
        .Atmosphere = 6460.0f,
        .RayleighDensity = 1.0f,
        .MieDensity = 1.0f,
        .OzoneDensity = 1.0f
    }};
}

ShadingSettings ShadingSettings::Default()
{
    return {{
        .EnvironmentPower = 1.0f,
        .SoftShadows = false,
        .MaxLightCullDistance = 1.0f,
        .VolumetricCloudShadowStrength = 0.35f,
    }};
}

ViewInfoGPU ViewInfoGPU::Default()
{
    return {{
        .Atmosphere = AtmosphereSettings::EarthDefault(),
        .Shading = ShadingSettings::Default()
    }};
}
