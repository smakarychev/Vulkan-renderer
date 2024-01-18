#version 460

#include "common.shader_header"

#extension GL_ARB_shader_draw_parameters : enable

@dynamic
layout(set = 0, binding = 0) uniform camera_buffer {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} u_camera_buffer;

layout(set = 0, binding = 1) readonly buffer position_buffer {
    Position positions[];
} u_position_buffer;

layout(set = 0, binding = 2) readonly buffer uv_buffer {
    UV uvs[];
} u_uv_buffer;

layout(std430, set = 1, binding = 0) readonly buffer object_buffer {
    object_data objects[];
} u_object_buffer;

@dynamic
layout(std430, set = 1, binding = 1) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_command_buffer;

layout(location = 0) out uint vert_command_id;
layout(location = 1) out uint vert_triangle_offset;
layout(location = 2) out vec2 vert_uv;

void main() {
    IndirectCommand command = u_command_buffer.commands[gl_DrawIDARB];
    uint object_index = command.render_object;
    
    Position position = u_position_buffer.positions[gl_VertexIndex];
    vec3 position_v = vec3(position.x, position.y, position.z);
    
    UV uv = u_uv_buffer.uvs[gl_VertexIndex];
    vec2 uv_v = vec2(uv.u, uv.v);
    
    gl_Position = u_camera_buffer.view_projection * u_object_buffer.objects[object_index].model * vec4(position_v, 1.0);
    vert_command_id = gl_DrawIDARB;
    vert_triangle_offset = command.firstIndex / 3;
    vert_uv = uv_v;
}