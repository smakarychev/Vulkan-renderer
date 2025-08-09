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

layout(scalar, set = 1, binding = 2) uniform view_info {
    ViewInfo view;
} u_view_info;

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

void main() {
    const float depth = textureLod(sampler2D(u_depth, u_sampler), vertex_uv, 0).r;
    const uint zbin_index = get_zbin_index(depth, u_view_info.view.near, u_view_info.view.max_light_cull_distance);
    
    const uint light_min = uint(u_zbins.bins[zbin_index].min);
    const uint light_max = uint(u_zbins.bins[zbin_index].max);
    
    const uint bin_min = u_use_zbins ? light_min / BIN_BIT_SIZE : 0;
    const uint bin_max = u_use_zbins ? light_max / BIN_BIT_SIZE : BIN_COUNT - 1;
    
    const uint tile_index = get_tile_index(vertex_uv, u_view_info.view.resolution);
    const Tile tile = u_tiles.tiles[tile_index];
    uint light_count = 0;
    for (uint i = bin_min; i <= bin_max; i++) {
        light_count += bitCount(tile.bins[i]);
    }
    
    const uint MAX_LIGHTS_TO_COLOR = 25;
    out_color = vec4(color_heatmap(clamp(float(light_count) / MAX_LIGHTS_TO_COLOR, 0.0f, 1.0f)), 1.0f);
}