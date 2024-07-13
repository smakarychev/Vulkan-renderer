#pragma once

#include "types.h"

#include <glm/glm.hpp>

struct AABB
{
    glm::vec3 Min{0.0f};
    glm::vec3 Max{0.0f};

    void Merge(const AABB& other)
    {
        Min = glm::min(Min, other.Min);
        Max = glm::max(Max, other.Max);
    }
};

struct Sphere
{
    glm::vec3 Center{0.0f};
    f32 Radius{0.0f};

    void Merge(const Sphere& other)
    {
        f32 distance = glm::length(Center - other.Center);

        if (distance <= std::abs(Radius - other.Radius))
        {
            *this = Radius > other.Radius ? *this : other;
            return;
        }

        Center = (Radius * Center + Radius * Center) / (Radius + Radius);
        Radius = (distance + Radius + Radius) / 2.0f;
    }
};
