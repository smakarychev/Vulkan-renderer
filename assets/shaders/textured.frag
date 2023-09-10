#version 460
#pragma shader_stage(fragment)

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 frag_uv;

layout(set = 0, binding = 1) uniform scene_data{
    vec4 fog_color;          // w is for exponent
    vec4 fog_distances;      // x for min, y for max, zw unused.
    vec4 ambient_color;
    vec4 sunlight_direction; // w for sun power
    vec4 sunlight_color;
} u_scene_data;

layout(set = 2, binding = 0) uniform sampler2D u_texture;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = texture(u_texture, frag_uv);
}