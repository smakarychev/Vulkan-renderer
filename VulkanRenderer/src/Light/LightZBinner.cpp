#include "LightZBinner.h"

#include "LightFrustumCuller.h"
#include "SceneLight.h"
#include "Core/Camera.h"

ZBins LightZBinner::ZBinLights(SceneLight& light, const Camera& camera)
{
    ZBins bins = {};
    
    /* first cull and sort lights by depth */
    LightFrustumCuller::CullDepthSort(light, camera);

    /* bins are uniform in z */
    f32 zSpan = camera.GetFrustumPlanes().Far - camera.GetFrustumPlanes().Near;
    for (u16 lightIndex = 0; lightIndex < (u16)light.GetVisiblePointLightCount(); lightIndex++)
    {
        auto& pointLight = light.GetVisiblePointLights()[lightIndex];
        f32 distanceMin = -(camera.GetView() * glm::vec4{pointLight.Position, 1.0f}).z - pointLight.Radius;
        f32 distanceMax = -(camera.GetView() * glm::vec4{pointLight.Position, 1.0f}).z + pointLight.Radius;
        distanceMin = std::max(distanceMin, 0.0f);
        ASSERT(distanceMax >= 0.0, "Lights supposed to be frustum culled before z-binning")

        u32 binMinIndex = (u32)(distanceMin / zSpan * LIGHT_TILE_BINS_Z);
        u32 binMaxIndex = (u32)(distanceMax / zSpan * LIGHT_TILE_BINS_Z);

        for (u32 binIndex = binMinIndex; binIndex <= binMaxIndex; binIndex++)
        {
            auto& bin = bins.Bins[binIndex];
            bin.LightMin = std::min(bin.LightMin, lightIndex);
            bin.LightMax = std::max(bin.LightMax, lightIndex);
        }
    }

    return bins;
}
