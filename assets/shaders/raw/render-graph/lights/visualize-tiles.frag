#version 460

#include "common.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 vertex_uv;

layout(set = 1, binding = 0) readonly buffer tiles {
    Tile tiles[];
} u_tiles;

layout(scalar, set = 1, binding = 1) uniform camera {
    CameraGPU camera;
} u_camera;

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
    const uvec2 tile_count = uvec2(ceil(u_camera.camera.resolution.xy / uvec2(LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y)));
    const uvec2 tile_index = uvec2(vec2(vertex_uv.x, 1.0f - vertex_uv.y) * uvec2(tile_count.x, tile_count.y));

    const Tile tile = u_tiles.tiles[tile_index.x + tile_index.y * tile_count.x];
    uint light_count = 0;
    for (uint i = 0; i < BIN_COUNT; i++) {
        light_count += bitCount(tile.bins[i]);
    }

    const uint MAX_LIGHTS_TO_COLOR = 25;
    out_color = vec4(color_heatmap(clamp(float(light_count) / MAX_LIGHTS_TO_COLOR, 0.0f, 1.0f)), 1.0f);
}