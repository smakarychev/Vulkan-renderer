#pragma once

#include "Settings.h"
#include "RenderGraph/Passes/Generated/Types/DirectionalLightUniform.generated.h"
#include "RenderGraph/Passes/Generated/Types/PointLightUniform.generated.h"

#include <CoreLib/types.h>
#include <CoreLib/Math/Geometry.h>

#include <glm/glm.hpp>
#include <array>

struct DirectionalLight : gen::DirectionalLight
{
    bool operator==(const DirectionalLight& other) const;
};

// todo: remove me in favour of sphere and tube lights

struct PointLight : gen::PointLight
{
    bool operator==(const PointLight& other) const;
};

struct LightsInfo
{
    u32 DirectionalLightCount{};
    u32 PointLightCount{};

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
