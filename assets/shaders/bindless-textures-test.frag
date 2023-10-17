#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 vert_normal;
layout(location = 1) in vec2 vert_uv;
layout(location = 2) in flat int vert_instance_id;

@dynamic
layout(set = 0, binding = 1) uniform scene_data{
    vec4 fog_color;          // w is for exponent
    vec4 fog_distances;      // x for min, y for max, zw unused.
    vec4 ambient_color;
    vec4 sunlight_direction; // w for sun power
    vec4 sunlight_color;
} u_scene_data;

@bindless
layout(set = 1, binding = 1) uniform texture2D u_textures[];

@immutable_sampler
layout(set = 0, binding = 2) uniform sampler u_sampler;

struct Material {
    vec4 albedo_color;
    uint albedo_texture_index;
    uint pad0;
    uint pad1;
    uint pad2;
};

layout(std140, set = 2, binding = 1) readonly buffer material_buffer{
    Material materials[];
} u_material_buffer;

layout(location = 0) out vec4 out_color;

void main() {
    Material material = u_material_buffer.materials[vert_instance_id];
    if (material.albedo_texture_index != -1)
        out_color = texture(nonuniformEXT(sampler2D(u_textures[nonuniformEXT(material.albedo_texture_index)], u_sampler)), vert_uv);
    else
        out_color = material.albedo_color;
    
    if (out_color.a < 0.5)
        discard;
   
    out_color = vec4(out_color.xyz * dot(normalize(vert_normal), normalize(vec3(u_scene_data.sunlight_direction))), out_color.w);
}