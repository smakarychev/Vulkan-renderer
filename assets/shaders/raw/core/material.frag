#version 460

struct Material {
    vec4 albedo_color;
    float metallic;
    float roughness;
    float pad0;
    uint albedo_texture_index;
    uint normal_texture_index;
    uint metallic_roughness_texture_index;
    uint ambient_occlusion_texture_index;
    uint emissive_texture_index;
};

layout(std430, set = 2, binding = 0) readonly buffer material_buffer{
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];


void main() {
    // this is just the easiest way to create material descriptors
}