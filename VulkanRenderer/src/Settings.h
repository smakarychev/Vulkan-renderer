#pragma once

#include "types.h"

#include <limits>

// todo: should be read as CVARs from config

namespace Settings
{
    void initCvars();
}

static constexpr u32 BUFFERED_FRAMES = 2;

static constexpr u32 MAX_CULL_VIEWS = 64;
static constexpr u32 MAX_CULL_GEOMETRIES = 4;

static constexpr u32 SUB_BATCH_COUNT = 8;
static constexpr u32 BATCH_OVERLAP = BUFFERED_FRAMES;

static constexpr u32 IRRADIANCE_RESOLUTION = 64;
static constexpr u32 PREFILTER_RESOLUTION = 256;
static constexpr u32 BRDF_RESOLUTION = 512;

static constexpr u32 SHADOW_MAP_RESOLUTION = 2048;
static constexpr u32 MAX_SHADOW_CASCADES = 5;
static constexpr u32 SHADOW_CASCADES = 4;

static constexpr f32 DEPTH_CONSTANT_BIAS = -1.0f;
static constexpr f32 DEPTH_SLOPE_BIAS = -1.75f;

static constexpr u32 LIGHT_CLUSTER_BINS_X = 16;
static constexpr u32 LIGHT_CLUSTER_BINS_Y = 9;
static constexpr u32 LIGHT_CLUSTER_BINS_Z = 24;
static constexpr u32 LIGHT_CLUSTER_BINS = LIGHT_CLUSTER_BINS_X * LIGHT_CLUSTER_BINS_Y * LIGHT_CLUSTER_BINS_Z;
static_assert(LIGHT_CLUSTER_BINS < std::numeric_limits<u16>::max(),
    "Shaders assume that there are less than 1 << 16 clusters. It is possible to change `active_clusters` type to u32");

static constexpr u32 LIGHT_TILE_SIZE_X = 8;
static constexpr u32 LIGHT_TILE_SIZE_Y = 8;
static constexpr u32 LIGHT_TILE_BINS_Z = 8096;
static_assert(LIGHT_TILE_SIZE_X >= 8 && LIGHT_TILE_SIZE_Y >= 8,
    "Shaders assume that light tile size is at least 8 pixels in each dimension");

// todo: I actually have cvar for that
static constexpr u32 VIEW_MAX_LIGHTS = 1024;
static constexpr u32 BIN_BIT_SIZE = 32;
static constexpr u32 BIN_COUNT = VIEW_MAX_LIGHTS / BIN_BIT_SIZE;

static constexpr bool LIGHT_CULLING = true;