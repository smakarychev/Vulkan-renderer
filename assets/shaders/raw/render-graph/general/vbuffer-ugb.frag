#version 460

#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier: require

layout(location = 0) in flat uint vertex_command_id;
layout(location = 1) in flat uint vertex_material_id;
layout(location = 2) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;

layout(std430, set = 1, binding = 3) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(scalar, set = 2, binding = 0) readonly buffer material_buffer{
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

layout(location = 0) out uint out_visibility_info;

void main() {
    const IndirectCommand command = u_commands.commands[vertex_command_id];
    const Material material = u_materials.materials[vertex_material_id];

    float alpha = material.albedo_color.a;
    alpha *= texture(nonuniformEXT(
        sampler2D(u_textures[material.albedo_texture_index], u_sampler)), vertex_uv).a;

    if (alpha < 0.5f)
        discard;

    VisibilityInfo info;
    info.instance_id = command.firstInstance;
    info.triangle_id = gl_PrimitiveID;
    out_visibility_info = pack_visibility(info);
}