#version 460

#include "common.shader_header"

layout(location = 0) in flat uint vert_command_id;
layout(location = 1) in flat uint vert_triangle_offset;

@dynamic
layout(std430, set = 2, binding = 0) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_command_buffer;

@dynamic
layout(std430, set = 2, binding = 1) readonly buffer triangle_buffer {
    uint triangles[];
} u_triangle_buffer;

layout(location = 0) out uint out_visibility_info;

void main() {
    IndirectCommand command = u_command_buffer.commands[vert_command_id];

    uint instance_id = command.firstInstance;
    uint triangle_id = u_triangle_buffer.triangles[vert_triangle_offset + gl_PrimitiveID];
    
    VisibilityInfo info;
    info.instance_id = instance_id;
    info.triangle_id = triangle_id;
    out_visibility_info = pack_visibility(info);
}