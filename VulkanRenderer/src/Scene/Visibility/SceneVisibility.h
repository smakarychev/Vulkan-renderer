#pragma once

#include <CoreLib/types.h>

struct SceneVisibilityHandle
{
    static constexpr u32 INVALID = ~0lu;
    u32 Handle{INVALID};

    auto operator<=>(const SceneVisibilityHandle&) const = default;
    operator u32() const { return Handle; }
};