#version 460
#pragma shader_stage(fragment)

layout(location = 0) in vec3 vert_normal;
layout(location = 1) in flat int vert_instance_id;


layout(set = 0, binding = 1) uniform scene_data{
    vec4 fog_color;          // w is for exponent
    vec4 fog_distances;      // x for min, y for max, zw unused.
    vec4 ambient_color;
    vec4 sunlight_direction; // w for sun power
    vec4 sunlight_color;
} dyn_u_scene_data;


layout(std140, set = 2, binding = 1) readonly buffer material_buffer{
    vec4 albedo_colors[];
} u_material_buffer;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = u_material_buffer.albedo_colors[vert_instance_id];
    if (out_color.a < 0.5)
        discard;
    out_color = vec4(out_color.xyz * dot(normalize(vert_normal), normalize(vec3(dyn_u_scene_data.sunlight_direction))), out_color.w);
}