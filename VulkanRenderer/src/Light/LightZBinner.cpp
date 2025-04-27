#include "LightZBinner.h"

#include "LightFrustumCuller.h"
#include "Core/Camera.h"
#include "Scene/SceneLight2.h"

ZBins LightZBinner::ZBinLights(SceneLight2& light, const Camera& camera)
{
    ZBins bins = {};
    
    /* first cull and sort lights by depth */
    LightFrustumCuller::CullDepthSort(light, camera);

    /* bins are uniform in z */
    f32 zSpan = camera.GetFrustumPlanes().Far - camera.GetFrustumPlanes().Near;
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
                    .Radius = commonLight.Radius};
                sphere.Center = glm::vec3{camera.GetView() * glm::vec4{sphere.Center, 1.0f}};
        
                f32 distanceMin = -sphere.Center.z - sphere.Radius;
                f32 distanceMax = -sphere.Center.z + sphere.Radius;
                distanceMin = std::max(distanceMin, 0.0f);
                ASSERT(distanceMax >= 0.0, "Lights supposed to be frustum culled before z-binning")

                u32 binMinIndex = (u32)(distanceMin / zSpan * LIGHT_TILE_BINS_Z);
                u32 binMaxIndex = (u32)(distanceMax / zSpan * LIGHT_TILE_BINS_Z);

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
