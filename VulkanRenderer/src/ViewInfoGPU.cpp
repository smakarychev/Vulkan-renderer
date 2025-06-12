#include "ViewInfoGPU.h"

#include "Core/Camera.h"
#include "Rendering/Image/ImageUtility.h"

CameraGPU CameraGPU::FromCamera(const Camera& camera, const glm::uvec2& resolution, ::VisibilityFlags visibilityFlags)
{
    u32 viewFlags = {};
    viewFlags |= (u32)(camera.GetType() == CameraType::Orthographic) << IS_ORTHOGRAPHIC_BIT;
    viewFlags |= (u32)enumHasAny(visibilityFlags, VisibilityFlags::ClampDepth) << CLAMP_DEPTH_BIT;
    const glm::uvec2 hizResolution = enumHasAny(visibilityFlags, VisibilityFlags::OcclusionCull) ?
        Images::floorResolutionToPowerOfTwo(resolution) : glm::uvec2(0);
    
    CameraGPU cameraGPU = {
        .ViewProjection = camera.GetViewProjection(),
        .Projection = camera.GetProjection(),
        .View = camera.GetView(),
        .Position = camera.GetPosition(),
        .Near = camera.GetFrustumPlanes().Near,
        .Forward = camera.GetForward(),
        .Far = camera.GetFrustumPlanes().Far,
        .InverseViewProjection = glm::inverse(camera.GetViewProjection()),
        .InverseProjection = glm::inverse(camera.GetProjection()),
        .InverseView = glm::inverse(camera.GetView()),
        .FrustumPlanes = camera.GetFrustumPlanes(),
        .ProjectionData = camera.GetProjectionData(),
        .Resolution = glm::vec2{resolution},
        .HiZResolution = hizResolution,
        .ViewFlagsGpu = viewFlags,
        .VisibilityFlags = visibilityFlags,
    };

    return cameraGPU;
}

AtmosphereSettings AtmosphereSettings::EarthDefault()
{
    return {
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
        .OzoneDensity = 1.0f};
}
