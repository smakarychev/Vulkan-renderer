#include "LightFrustumCuller.h"

#include <algorithm>
#include <vector>

#include "Light.h"
#include "SceneLight.h"
#include "Common/Geometry.h"
#include "Core/Camera.h"
#include "cvars/CVarSystem.h"

namespace
{
    std::vector<PointLight> getVisiblePointLights(SceneLight& light, const Camera& camera)
    {
        auto isVisiblePerspective = [](const Sphere& sphere, const FrustumPlanes& frustum)
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
        };
        auto isVisibleOrthographic = [](const Sphere& sphere, const FrustumPlanes& frustum,
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
        };

        u32 maxLightsPerFrustum = (u32)*CVars::Get().GetI32CVar({"Lights.FrustumMax"});
        
        FrustumPlanes frustum = camera.GetFrustumPlanes();
        ProjectionData projection = camera.GetProjectionData();

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

    void sortByDepth(std::vector<PointLight>& pointLights, const Camera& camera)
    {
        Plane sortPlane = camera.GetNearViewPlane();

        std::ranges::sort(pointLights, std::less<>{}, [&sortPlane](auto& light)
        {
            return sortPlane.SignedDistance(light.Position);
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
