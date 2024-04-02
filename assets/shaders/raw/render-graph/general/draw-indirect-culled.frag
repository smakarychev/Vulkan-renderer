#version 460

#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in flat uint vert_command_id;
layout(location = 1) in vec3 vertex_position;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec3 vertex_tangent;
layout(location = 4) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;

layout(std430, set = 1, binding = 6) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(std430, set = 1, binding = 7) readonly buffer triangle_buffer {
    uint8_t triangles[];
} u_triangles;

layout(set = 2, binding = 0) readonly buffer material_buffer{
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

layout(location = 0) out vec4 out_color;

void main() {
    IndirectCommand command = u_commands.commands[vert_command_id];
    uint object_index = command.render_object;
    Material material = u_materials.materials[object_index];

    vec4 albedo = material.albedo_color;
    albedo *= texture(nonuniformEXT(sampler2D(u_textures[material.albedo_texture_index], u_sampler)),
        vertex_uv).rgba;

    out_color = albedo;
}