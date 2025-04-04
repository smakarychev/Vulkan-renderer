#include "LightFrustumCuller.h"

#include "Light.h"
#include "SceneLight.h"
#include "Scene/SceneLight2.h"
#include "Math/Geometry.h"
#include "Core/Camera.h"
#include "cvars/CVarSystem.h"

#include <algorithm>
#include <vector>

namespace
{
    bool isVisiblePerspective(const Sphere& sphere, const FrustumPlanes& frustum)
    {
        bool visible = true;
        visible = visible && frustum.RightX * abs(sphere.Center.x) + sphere.Center.z * frustum.RightZ <
            sphere.Radius;
        visible = visible && frustum.TopY * abs(sphere.Center.y) + sphere.Center.z * frustum.TopZ <
            sphere.Radius;
        visible = visible &&
            sphere.Center.z - sphere.Radius <= -frustum.Near &&
            sphere.Center.z + sphere.Radius >= -frustum.Far;

        return visible;
    }
    
    bool isVisibleOrthographic(const Sphere& sphere, const FrustumPlanes& frustum,
            const ProjectionData& projection)
    {
        bool visible = true;
        visible = visible && abs(frustum.RightX * sphere.Center.x + projection.BiasX) <
            1.0f + frustum.RightX * sphere.Radius;
        visible = visible && abs(frustum.TopY * sphere.Center.y + projection.BiasY) <
            1.0f + frustum.TopY * sphere.Radius;
        visible = visible &&
            sphere.Center.z - sphere.Radius <= -frustum.Near &&
            sphere.Center.z + sphere.Radius >= -frustum.Far;

        return visible;
    }

    // todo: remove
    std::vector<PointLight> getVisiblePointLights(SceneLight& light, const Camera& camera)
    {
        const u32 maxLightsPerFrustum = (u32)*CVars::Get().GetI32CVar("Lights.FrustumMax"_hsv);
        
        const FrustumPlanes frustum = camera.GetFrustumPlanes();
        const ProjectionData projection = camera.GetProjectionData();

        std::vector<PointLight> visibleLights;
        visibleLights.reserve(light.GetPointLightCount());
        for (auto& pointLight : light.GetPointLights())
        {
            Sphere sphere = {
                .Center = pointLight.Position,
                .Radius = pointLight.Radius};
            sphere.Center = glm::vec3{camera.GetView() * glm::vec4{sphere.Center, 1.0f}};
        
            bool isVisible = camera.GetType() == CameraType::Perspective ?
                isVisiblePerspective(sphere, frustum) :
                isVisibleOrthographic(sphere, frustum, projection);
            if (isVisible)
                visibleLights.push_back(pointLight);
            if (visibleLights.size() == maxLightsPerFrustum)
                break;
        }

        return visibleLights;
    }
    
    std::vector<u32> getVisibleLights(const SceneLight2& light, const Camera& camera)
    {
        const u32 maxLightsPerFrustum = (u32)*CVars::Get().GetI32CVar("Lights.FrustumMax"_hsv);
        
        const FrustumPlanes frustum = camera.GetFrustumPlanes();
        const ProjectionData projection = camera.GetProjectionData();

        std::vector<u32> visibleLights;
        visibleLights.reserve(light.Count());
        for (u32 i = 0; i < light.Count(); i++)
        {
            const CommonLight& commonLight = light.Get(i);
            switch (commonLight.Type)
            {
            case LightType::Point:
                {
                    Sphere sphere = {
                        .Center = commonLight.PositionDirection,
                        .Radius = commonLight.Radius};
                    sphere.Center = glm::vec3{camera.GetView() * glm::vec4{sphere.Center, 1.0f}};
        
                    const bool isVisible = camera.GetType() == CameraType::Perspective ?
                        isVisiblePerspective(sphere, frustum) :
                        isVisibleOrthographic(sphere, frustum, projection);
                    if (isVisible)
                        visibleLights.push_back(i);
                    if (visibleLights.size() == maxLightsPerFrustum)
                        break;
                }
                break;
            case LightType::Directional:
            case LightType::Spot:
            default:
                visibleLights.push_back(i);
                break;
            }
        }

        return visibleLights;
    }

    // todo: remove
    void sortByDepth(std::vector<PointLight>& pointLights, const Camera& camera)
    {
        Plane sortPlane = camera.GetNearViewPlane();

        std::ranges::sort(pointLights, std::less<>{}, [&sortPlane](auto& light)
        {
            return sortPlane.SignedDistance(light.Position);
        });
    }
    
    void sortByDepth(const SceneLight2& light, std::vector<u32>& lightIndices, const Camera& camera)
    {
        Plane sortPlane = camera.GetNearViewPlane();

        std::ranges::sort(lightIndices, std::less<>{}, [&light, &sortPlane](u32 lightIndex) -> f32
        {
            const CommonLight& commonLight = light.Get(lightIndex);
            switch (commonLight.Type)
            {
            case LightType::Directional:
                return 0.0f;
            case LightType::Point:
                return sortPlane.SignedDistance(commonLight.PositionDirection);
            case LightType::Spot:
            default:
                ASSERT(false, "Unsupported light type")
                break;
            }
            std::unreachable();
        });
    }
}

void LightFrustumCuller::Cull(SceneLight& light, const Camera& camera)
{
    light.SetVisiblePointLights(getVisiblePointLights(light, camera));
}

void LightFrustumCuller::CullDepthSort(SceneLight& light, const Camera& camera)
{
    auto visiblePointLights = getVisiblePointLights(light, camera);
    sortByDepth(visiblePointLights, camera);
    
    light.SetVisiblePointLights(visiblePointLights);
}

void LightFrustumCuller::Cull(SceneLight2& light, const Camera& camera)
{
    light.SetVisibleLights(getVisibleLights(light, camera));
}

void LightFrustumCuller::CullDepthSort(SceneLight2& light, const Camera& camera)
{
    auto visibleLights = getVisibleLights(light, camera);
    sortByDepth(light, visibleLights, camera);

    light.SetVisibleLights(visibleLights);
}
