#include "../common.glsl"

#extension GL_EXT_shader_explicit_arithmetic_types: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8: require   
const uint MAX_VIEWS = 64;

#define BUCKET_BIT_SHIFT 6

struct VisibilityBucketBits
{
    // store uint64_t as 2 uints because of subgroup operations
    uint visibility[2];
};

struct VisibilityBucketIndex
{
    uint bucket;
    uint bit_high;
    uint bit_low;
};

bool is_orthographic(ViewInfo view) {
    return ((view.view_flags >> VIEW_IS_ORTHOGRAPHIC_BIT) & 1u) == 1;
}

bool is_depth_clamped(ViewInfo view) {
    return ((view.view_flags >> VIEW_CLAMP_DEPTH_BIT) & 1u) == 1;
}