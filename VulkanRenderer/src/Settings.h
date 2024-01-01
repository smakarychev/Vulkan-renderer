#pragma once

#include "types.h"

// todo: should be read as CVARs from config

static constexpr u32 BUFFERED_FRAMES{2};

static constexpr u32 SUB_BATCH_COUNT{8};
static constexpr u32 CULL_DRAW_BATCH_OVERLAP{2};


static constexpr u32 MAX_OBJECTS{1'000'000};
static constexpr u32 MAX_DRAW_INDIRECT_CALLS{MAX_OBJECTS};
static constexpr u32 BINDLESS_TEXTURES_COUNT{MAX_OBJECTS};