#include "rendererpch.h"

#include "LightZBinner.h"

#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "Scene/SceneLight.h"

ZBins LightZBinner::ZBinLights(SceneLight& light, const Camera& camera)
{
    ZBins bins = {};

    /* bins are uniform in z */
    const f32 maxLightCullDistance = *CVars::Get().GetF32CVar("Renderer.Limits.MaxLightCullDistance"_hsv);
    const FrustumPlanes frustum = camera.GetFrustumPlanes(maxLightCullDistance);
    f32 zSpan = frustum.Far - frustum.Near;
    u16 pointLightIndex = 0;
    for (u32 visibleLight : light.VisibleLights())
    {
        auto& commonLight = light.Get(visibleLight);
        switch (commonLight.Type)
        {
        case LightType::Point:
            {
                Sphere sphere = {
                    .Center = commonLight.PositionDirection,
                    .Radius = commonLight.Radius
                };
                sphere.Center = glm::vec3{camera.GetView() * glm::vec4{sphere.Center, 1.0f}};
        
                const f32 distanceMin = std::max(-sphere.Center.z - sphere.Radius, 0.0f);
                const f32 distanceMax = -sphere.Center.z + sphere.Radius;

                const u32 binMinIndex = (u32)(distanceMin / zSpan * LIGHT_TILE_BINS_Z);
                const u32 binMaxIndex = std::min((u32)(distanceMax / zSpan * LIGHT_TILE_BINS_Z), LIGHT_TILE_BINS_Z - 1);

                for (u32 binIndex = binMinIndex; binIndex <= binMaxIndex; binIndex++)
                {
                    auto& bin = bins.Bins[binIndex];
                    bin.LightMin = std::min(bin.LightMin, pointLightIndex);
                    bin.LightMax = std::max(bin.LightMax, pointLightIndex);
                }
                pointLightIndex++;
            }
            break;
        case LightType::Directional:
        case LightType::Spot:
        default:
            break;
        }
    }
    
    return bins;
}
