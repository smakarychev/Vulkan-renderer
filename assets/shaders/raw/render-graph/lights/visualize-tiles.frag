#version 460

#include "common.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;
layout(set = 1, binding = 0) uniform texture2D u_depth;

layout(set = 1, binding = 1) readonly buffer tiles {
    Tile tiles[];
} u_tiles;

layout(scalar, set = 1, binding = 2) uniform camera {
    CameraGPU camera;
} u_camera;

layout(scalar, set = 1, binding = 3) readonly buffer zbins {
    ZBin bins[];
} u_zbins;

layout(push_constant) uniform push_constants {
    bool u_use_zbins;
};

vec3 color(float t) {
    const vec3 a = vec3(0.5f, 0.5f, 0.5f);		
    const vec3 b = vec3(0.5f, 0.5f, 0.5f);	
    const vec3 c = vec3(1.0f, 1.0f, 1.0f);	
    const vec3 d = vec3(0.3f, 0.2f, 0.2f);
    
    return a + b * cos(2.0f * 3.1415f * (c * t + d));
}

vec3 color_heatmap(float t) {
    t = t * 3.1415 * 0.5f;
    return vec3(sin(t), sin(2.0f * t), cos(t));
}

uint hash(uint x) {
    uint state = x * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

    return (word >> 22u) ^ word;
}

vec3 color_hash(uint x) {
    const uint hash_val = hash(x);

    return vec3(
    float(hash_val & 255u) / 255.0f,
    float((hash_val >> 8) & 255u) / 255.0f,
    float((hash_val >> 16) & 255u) / 255.0f);
}

void main() {
    const float depth = -linearize_reverse_z(
        textureLod(sampler2D(u_depth, u_sampler), vertex_uv, 0).r,
        u_camera.camera.near,
        u_camera.camera.far);
    const float depth_range = u_camera.camera.far - u_camera.camera.near;
    const uint z_bin_index = uint(depth / depth_range * LIGHT_TILE_BINS_Z);
    
    const uint light_min = uint(u_zbins.bins[z_bin_index].min);
    const uint light_max = uint(u_zbins.bins[z_bin_index].max);
    
    const uint bin_min = u_use_zbins ? light_min / BIN_BIT_SIZE : 0;
    const uint bin_max = u_use_zbins ? light_max / BIN_BIT_SIZE : BIN_COUNT - 1;
    
    const uvec2 tile_size = uvec2(ceil(u_camera.camera.resolution / vec2(LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y)));
    const uvec2 tile_index = uvec2(floor(vec2(vertex_uv.x, 1.0f - vertex_uv.y) * tile_size));
    
    const Tile tile = u_tiles.tiles[tile_index.x + tile_index.y * tile_size.x];
    uint light_count = 0;
    for (uint i = bin_min; i <= bin_max; i++) {
        light_count += bitCount(tile.bins[i]);
    }
    
    const uint MAX_LIGHTS_TO_COLOR = 25;
    out_color = vec4(color_heatmap(clamp(float(light_count) / MAX_LIGHTS_TO_COLOR, 0.0f, 1.0f)), 1.0f);
}