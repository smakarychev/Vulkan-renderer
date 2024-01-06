#version 460

#include "common.shader_header"

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in flat uint vert_command_id;
layout(location = 1) in flat uint vert_triangle_offset;
layout(location = 2) in vec2 vert_uv;

@dynamic
layout(std430, set = 1, binding = 1) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_command_buffer;

@dynamic
layout(std430, set = 1, binding = 2) readonly buffer triangle_buffer {
    uint8_t triangles[];
} u_triangle_buffer;

layout(std430, set = 1, binding = 3) readonly buffer material_buffer{
    Material materials[];
} u_material_buffer;

@immutable_sampler
layout(set = 2, binding = 0) uniform sampler u_sampler;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

layout(location = 0) out uint out_visibility_info;

void main() {
    IndirectCommand command = u_command_buffer.commands[vert_command_id];
    uint object_index = command.render_object;
    Material material = u_material_buffer.materials[object_index];

    float alpha;
    if (material.albedo_texture_index != ~0)
        alpha = texture(nonuniformEXT(
            sampler2D(u_textures[nonuniformEXT(material.albedo_texture_index)], u_sampler)), vert_uv).a;
    else
        alpha = material.albedo_color.a;

    if (alpha < 0.9)
        discard;

    uint instance_id = command.firstInstance;
    uint triangle_id = uint(u_triangle_buffer.triangles[vert_triangle_offset + gl_PrimitiveID]);
    
    VisibilityInfo info;
    info.instance_id = instance_id;
    info.triangle_id = triangle_id;
    out_visibility_info = pack_visibility(info);
}