#include "rendererpch.h"
#include "SceneAsset.h"

#include "Light/Light.h"

namespace lux
{
void SceneLightInfo::AddLight(const DirectionalLight& light)
{
    Lights.push_back({
        .Type = LightType::Directional,
        .PositionDirection = light.Direction,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius,
    });
}

void SceneLightInfo::AddLight(const PointLight& light)
{
    Lights.push_back({
        .Type = LightType::Point,
        .PositionDirection = light.Position,
        .Color = light.Color,
        .Intensity = light.Intensity,
        .Radius = light.Radius,
    });
}

void SceneAsset::AddLight(const DirectionalLight& light)
{
    const u32 lightIndex = (u32)Lights.Lights.size();
    Lights.AddLight(light);

    Hierarchy.Nodes.push_back(SceneHierarchyNode{
        .Type = SceneHierarchyNodeType::Light,
        .Depth = 0,
        .Parent = {SceneHierarchyHandle::INVALID},
        .LocalTransform = Lights.Lights.back().GetTransform(),
        .PayloadIndex = lightIndex
    });
}

void SceneAsset::AddLight(const PointLight& light)
{
    const u32 lightIndex = (u32)Lights.Lights.size();
    Lights.AddLight(light);

    Hierarchy.Nodes.push_back(SceneHierarchyNode{
        .Type = SceneHierarchyNodeType::Light,
        .Depth = 0,
        .Parent = {SceneHierarchyHandle::INVALID},
        .LocalTransform = Lights.Lights.back().GetTransform(),
        .PayloadIndex = lightIndex
    });
}

Transform3d CommonLight::GetTransform() const
{
    switch (Type)
    {
    case LightType::Directional:
        return Transform3d {
            .Orientation = glm::quatLookAt(PositionDirection, glm::vec3(0.0f, 1.0f, 0.0f)),
        };
    case LightType::Point:
        return Transform3d {
            .Position = PositionDirection,
        };
    case LightType::Spot:
        ASSERT(false, "Spot light is not supported")
        break;
    default:
        ASSERT(false, "Light type is not supported")
        break;
    }
    
    std::unreachable();
}
}
