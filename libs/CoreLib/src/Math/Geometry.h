#pragma once

#include "types.h"

#include <glm/glm.hpp>

#include "Transform.h"

struct Sphere;

struct AABB
{
    glm::vec3 Min{0.0f};
    glm::vec3 Max{0.0f};

    AABB Merge(const AABB& other) const;

    AABB Transform(const Transform3d& transform3d) const;

    static AABB FromSphere(const Sphere& sphere);
};

struct Sphere
{
    glm::vec3 Center{0.0f};
    f32 Radius{0.0f};

    Sphere Merge(const Sphere& other) const;
};

struct Plane
{
    glm::vec3 Normal{0.0f, 1.0f, 0.0f};
    f32 Offset{0.0f};

    constexpr f32 SignedDistance(const glm::vec3& point) const;

    static Plane ByPointAndNormal(const glm::vec3& point, const glm::vec3& normal);
};

constexpr f32 Plane::SignedDistance(const glm::vec3& point) const
{
    return glm::dot(Normal, point) + Offset;
}

inline AABB AABB::Merge(const AABB& other) const
{
    return {
        .Min = glm::min(Min, other.Min),
        .Max = glm::max(Max, other.Max)};
}

inline AABB AABB::Transform(const Transform3d& transform3d) const
{
    auto&& [position, orientation, scale] = transform3d;
        
    if (orientation == glm::quat{1.0f, 0.0f, 0.0f, 0.0f})
    {
        glm::vec3 min = Min * scale + position;
        glm::vec3 max = Max * scale + position;
            
        return {.Min = min, .Max = max};
    }
        
    const glm::mat4 model = transform3d.ToMatrix();

    glm::vec3 corners[8] = {
        glm::vec3(Min.x, Min.y, Min.z),
        glm::vec3(Min.x, Min.y, Max.z),
        glm::vec3(Min.x, Max.y, Min.z),
        glm::vec3(Min.x, Max.y, Max.z),
        glm::vec3(Max.x, Min.y, Min.z),
        glm::vec3(Max.x, Min.y, Max.z),
        glm::vec3(Max.x, Max.y, Min.z),
        glm::vec3(Max.x, Max.y, Max.z)
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

inline AABB AABB::FromSphere(const Sphere& sphere)
{
    return {
        .Min = sphere.Center - sphere.Radius,
        .Max = sphere.Center + sphere.Radius};
}

inline Sphere Sphere::Merge(const Sphere& other) const
{
    const glm::vec3 distanceVec = Center - other.Center;
    const f32 distance = glm::length(distanceVec);

    if (distance <= std::abs(Radius - other.Radius))
    {
        return Radius > other.Radius ? *this : other;
    }

    return {
        .Center = (Center + other.Center + (Radius - other.Radius) * distanceVec / distance) * 0.5f,
        .Radius = (distance + Radius + other.Radius) * 0.5f};
}

inline Plane Plane::ByPointAndNormal(const glm::vec3& point, const glm::vec3& normal)
{
    Plane plane = {};
    plane.Normal = glm::normalize(normal);
    plane.Offset = -glm::dot(plane.Normal, point);

    return plane;
}
