#version 460

#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier: enable

layout(location = 0) in flat uint vertex_material_id;
layout(location = 1) in vec3 vertex_position;
layout(location = 2) in vec2 vertex_uv;

layout(std430, set = 2, binding = 0) readonly buffer material {
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

void main() {
#ifdef ALPHA_TEST
    const Material material = u_materials.materials[vertex_material_id];
    if (texture(nonuniformEXT(sampler2D(u_textures[material.albedo_texture_index], u_sampler)), vertex_uv).a < 0.5f)
        discard;
#endif
}