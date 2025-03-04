#pragma once

#include "types.h"

#include <glm/glm.hpp>
#include <array>

#include "Settings.h"
#include "common/Geometry.h"

struct DirectionalLight
{
    glm::vec3 Direction{0.0f, -1.0f, 0.0f};
    glm::vec3 Color{1.0f};
    f32 Intensity{1.0f};
    f32 Size{1.0f};

    bool operator==(const DirectionalLight& other) const;
};

// todo: remove me in favour of sphere and tube lights

struct PointLight
{
    glm::vec3 Position{0.0f};
    glm::vec3 Color{1.0f};
    f32 Intensity{1.0f};
    f32 Radius{1.0f};

    bool operator==(const PointLight& other) const;
};

struct LightsInfo
{
    u32 PointLightCount;

    auto operator<=>(const LightsInfo& other) const = default;
};

struct LightCluster
{
    glm::vec4 Min;
    glm::vec4 Max;
    std::array<u32, BIN_COUNT> Bins;
};

struct LightTile
{
    std::array<Plane, 4> Planes;
    std::array<u32, BIN_COUNT> Bins;
};
