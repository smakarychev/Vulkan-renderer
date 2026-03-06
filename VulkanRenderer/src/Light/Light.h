#pragma once

#include "RenderGraph/Passes/Generated/Types/DirectionalLightUniform.generated.h"
#include "RenderGraph/Passes/Generated/Types/PointLightUniform.generated.h"
#include "RenderGraph/Passes/Generated/Types/LightClusterUniform.generated.h"
#include "RenderGraph/Passes/Generated/Types/LightTileUniform.generated.h"

#include <CoreLib/types.h>

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

struct LightCluster : gen::LightCluster{};
struct LightTile : gen::LightTile{};
