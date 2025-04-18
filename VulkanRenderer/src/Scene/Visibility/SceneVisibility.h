#pragma once

#include "SceneView.h"

using SceneVisibilityBucket = u64;

struct SceneVisibilityHandle
{
    static constexpr u32 INVALID = ~0lu;
    u32 Handle{INVALID};

    auto operator<=>(const SceneVisibilityHandle&) const = default;
    operator u32() const { return Handle; }
};