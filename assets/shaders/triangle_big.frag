#version 460
#pragma shader_stage(fragment)

layout(location = 0) in vec3 vert_color;
layout(location = 1) in vec3 vert_normal;


layout(set = 0, binding = 1) uniform scene_data{
    vec4 fog_color;          // w is for exponent
    vec4 fog_distances;      // x for min, y for max, zw unused.
    vec4 ambient_color;
    vec4 sunlight_direction; // w for sun power
    vec4 sunlight_color;
} dyn_u_scene_data;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(vert_color * dot(normalize(vert_normal), normalize(vec3(dyn_u_scene_data.sunlight_direction))), 1.0);
}