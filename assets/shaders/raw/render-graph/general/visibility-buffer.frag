#version 460

#include "common.glsl"

#extension GL_EXT_nonuniform_qualifier: require

layout(constant_id = 0) const bool COMPOUND_INDEX = false;

layout(location = 0) in flat uint vertex_command_id;
layout(location = 1) in vec2 vertex_uv;

@immutable_sampler
layout(set = 0, binding = 0) uniform sampler u_sampler;

layout(std430, set = 1, binding = 4) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(std430, set = 1, binding = 5) readonly buffer triangle_buffer {
    uint8_t triangles[];
} u_triangles;

layout(std430, set = 2, binding = 0) readonly buffer material_buffer{
    Material materials[];
} u_materials;

@bindless
layout(set = 2, binding = 1) uniform texture2D u_textures[];

layout(location = 0) out uint out_visibility_info;

void main() {
    const IndirectCommand command = u_commands.commands[vertex_command_id];
    const uint object_index = command.render_object;
    const Material material = u_materials.materials[object_index];

    float alpha = material.albedo_color.a;
    alpha *= texture(nonuniformEXT(
        sampler2D(u_textures[material.albedo_texture_index], u_sampler)), vertex_uv).a;

    if (alpha < 0.5f)
        discard; 

    uint instance_id;
    uint triangle_id;
    if (COMPOUND_INDEX) {
        instance_id = vertex_command_id;
        triangle_id = uint(u_triangles.triangles[gl_PrimitiveID]);
    }
    else {
        instance_id = command.firstInstance;
        triangle_id = gl_PrimitiveID;
    }
    
    VisibilityInfo info;
    info.instance_id = instance_id;
    info.triangle_id = triangle_id;
    out_visibility_info = pack_visibility(info);
}