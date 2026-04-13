#include "rendererpch.h"
#include "SceneAsset.h"

namespace lux
{
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
