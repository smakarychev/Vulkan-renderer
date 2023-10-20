#pragma once

#include "types.h"

// todo: should be read as CVARs from config

static constexpr u32 BUFFERED_FRAMES{2};

static constexpr u32 COMPUTE_PARTICLE_COUNT{256 * 16};

static constexpr u32 MAX_OBJECTS{20'000};
static constexpr u32 MAX_DRAW_INDIRECT_CALLS{MAX_OBJECTS};
static constexpr u32 BINDLESS_TEXTURES_COUNT{MAX_OBJECTS};