#version 460

#include "common.shader_header"

#extension GL_ARB_shader_draw_parameters : enable

@binding : 0
layout(location = 0) in vec3 a_position;

@dynamic
layout(set = 0, binding = 0) uniform camera_buffer {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} u_camera_buffer;

layout(std430, set = 1, binding = 0) readonly buffer object_buffer {
    object_data objects[];
} u_object_buffer;

@dynamic
layout(std430, set = 2, binding = 0) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_command_buffer;

layout(location = 0) out uint vert_command_id;
layout(location = 1) out uint vert_triangle_offset;

void main() {
    IndirectCommand command = u_command_buffer.commands[gl_DrawIDARB];
    uint object_index = command.render_object;
    gl_Position = u_camera_buffer.view_projection * u_object_buffer.objects[object_index].model * vec4(a_position, 1.0);
    vert_command_id = gl_DrawIDARB;
    vert_triangle_offset = command.firstIndex / 3;
}