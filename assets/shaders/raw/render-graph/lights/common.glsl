#include "../../light.glsl"
#include "../../camera.glsl"
#include "../../utility.glsl"
#include "../common.glsl"

// todo: make defines, so i can set them on compilation
const uint LIGHT_CLUSTER_BINS_X = 60;
const uint LIGHT_CLUSTER_BINS_Y = 32;
const uint LIGHT_CLUSTER_BINS_Z = 18;

const uint LIGHT_TILE_SIZE_X = 8;
const uint LIGHT_TILE_SIZE_Y = 8;
const uint LIGHT_TILE_BINS_Z = 8096;

const uint BIN_DISPATCH_SIZE = 256;

uint slice_index(float depth, float n, float f, uint count) {
    const float z = (n - f) * depth - n;
    const float log_f_n_inv = 1.0f / log(f / n);

    return uint(floor(log(f) * count * log_f_n_inv - log(-z) * count * log_f_n_inv));
}

uint slice_index_depth_linear(float z, float n, float f) {
    const float log_f_n_inv = 1.0f / log(f / n);

    return LIGHT_CLUSTER_BINS_Z -
        uint(floor(log(f) * LIGHT_CLUSTER_BINS_Z * log_f_n_inv - log(-z) * LIGHT_CLUSTER_BINS_Z * log_f_n_inv)) - 1;
}

uint get_cluster_index(vec2 uv, uint slice) {
    const uvec3 indices = uvec3(vec2(uv.x, 1.0f - uv.y) * uvec2(LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y), slice);

    return indices.x +
        indices.y * LIGHT_CLUSTER_BINS_X +
        indices.z * LIGHT_CLUSTER_BINS_X * LIGHT_CLUSTER_BINS_Y;
}

uint get_zbin_index(float depth, float near, float far) {
    const float z = -linearize_reverse_z(depth, near, far);
    const float depth_range = far - near;
    
    return uint(z / depth_range * LIGHT_TILE_BINS_Z);
}

uint get_tile_index(vec2 uv, vec2 resolution) {
    const uvec2 tile_size = uvec2(floor(resolution / vec2(LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y)));
    const uvec2 tile_index = uvec2(floor(vec2(uv.x, 1.0f - uv.y) * tile_size));

    return tile_index.x + tile_index.y * tile_size.x;
}

vec3 line_plane_intersection(vec3 dir, float z) {
    return vec3(dir.xy * z / dir.z, z);
}

float distance_aabb_squared(vec3 point, vec4 min, vec4 max) {
    float distance2 = 0.0f;
    for (uint i = 0; i < 3; i++) {
        const float c = point[i];
        if (c < min[i])
            distance2 += (min[i] - c) * (min[i] - c);
        if (c > max[i])
            distance2 += (max[i] - c) * (max[i] - c);
    }

    return distance2;
}