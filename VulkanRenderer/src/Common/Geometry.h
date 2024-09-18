#pragma once

#include "types.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

struct AABB
{
    glm::vec3 Min{0.0f};
    glm::vec3 Max{0.0f};

    void Merge(const AABB& other)
    {
        Min = glm::min(Min, other.Min);
        Max = glm::max(Max, other.Max);
    }

    static AABB Transform(const AABB& aabb,
        const glm::vec3& position, const glm::quat& orientation, const glm::vec3& scale)
    {
        if (orientation == glm::quat{})
        {
            glm::vec3 min = aabb.Min * scale + position;
            glm::vec3 max = aabb.Max * scale + position;
            
            return {.Min = min, .Max = max};
        }
        
        glm::mat4 model = glm::translate(glm::mat4{1.0f}, position) * 
            glm::toMat4(orientation) * 
            glm::scale(glm::mat4{1.0f}, scale);

        glm::vec3 corners[8] = {
            glm::vec3(aabb.Min.x, aabb.Min.y, aabb.Min.z),
            glm::vec3(aabb.Min.x, aabb.Min.y, aabb.Max.z),
            glm::vec3(aabb.Min.x, aabb.Max.y, aabb.Min.z),
            glm::vec3(aabb.Min.x, aabb.Max.y, aabb.Max.z),
            glm::vec3(aabb.Max.x, aabb.Min.y, aabb.Min.z),
            glm::vec3(aabb.Max.x, aabb.Min.y, aabb.Max.z),
            glm::vec3(aabb.Max.x, aabb.Max.y, aabb.Min.z),
            glm::vec3(aabb.Max.x, aabb.Max.y, aabb.Max.z)
        };

        for (auto& c : corners)
            c = glm::vec3(model * glm::vec4(c, 1.0f));

        glm::vec3 min = glm::vec3(std::numeric_limits<f32>::max());
        glm::vec3 max = -min;
        for (auto& c : corners)
        {
            min = glm::min(min, c);
            max = glm::max(max, c);
        }

        return {.Min = min, .Max = max};
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

struct Plane
{
    glm::vec3 Normal{0.0f};
    f32 Offset{0.0f};

    constexpr f32 SignedDistance(const glm::vec3& point) const
    {
        return glm::dot(Normal, point) + Offset;
    }

    static Plane ByPointAndNormal(const glm::vec3& point, const glm::vec3& normal)
    {
        Plane plane = {};
        plane.Normal = glm::normalize(normal);
        plane.Offset = -glm::dot(plane.Normal, point);

        return plane;
    }
};
