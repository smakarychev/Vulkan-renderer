#pragma once

#include "types.h"

// todo: should be read as CVARs from config

static constexpr u32 BUFFERED_FRAMES{2};

static constexpr u32 SUB_BATCH_COUNT{8};

static constexpr u32 IRRADIANCE_RESOLUTION = 64;
static constexpr u32 PREFILTER_RESOLUTION = 256;
static constexpr u32 BRDF_RESOLUTION = 512;

static constexpr u32 SHADOW_MAP_RESOLUTION = 2048;
