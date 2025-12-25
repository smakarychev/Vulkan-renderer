#pragma once

#include "types.h"

#include <glm/glm.hpp>
#include <array>

#include "Settings.h"
#include "Math/Geometry.h"
#include "RenderGraph/Passes/Generated/Types/DirectionalLightUniform.generated.h"
#include "RenderGraph/Passes/Generated/Types/PointLightUniform.generated.h"

struct DirectionalLight : gen::DirectionalLight
{
    bool operator==(const DirectionalLight& other) const;
};

// todo: remove me in favour of sphere and tube lights

struct PointLight : gen::PointLight
{
    bool operator==(const PointLight& other) const;
};

// todo: remove me completely once fully slang
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
