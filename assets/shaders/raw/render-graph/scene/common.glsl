#include "../general/common.glsl"

#extension GL_EXT_scalar_block_layout: require
#extension GL_KHR_shader_subgroup_ballot: require
#extension GL_KHR_shader_subgroup_arithmetic: require
#extension GL_EXT_shader_explicit_arithmetic_types_int64: require
#extension GL_EXT_shader_subgroup_extended_types_int64: require

#define BUCKETS_PER_SET 64

struct DrawInfo {
    uint count;
};

/* must have scalar layout */
struct MeshletBucketInfo {
    uint index;
    uint64_t buckets;
};