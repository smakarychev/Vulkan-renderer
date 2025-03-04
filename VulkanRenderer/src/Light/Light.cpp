#include "Light.h"

bool DirectionalLight::operator==(const DirectionalLight& other) const
{
    return
        Direction == other.Direction &&
        Color == other.Color &&
        Intensity == other.Intensity &&
        Size == other.Size;
}

bool PointLight::operator==(const PointLight& other) const
{
    return
        Position == other.Position &&
        Color == other.Color &&
        Intensity == other.Intensity &&
        Radius == other.Radius;
}
