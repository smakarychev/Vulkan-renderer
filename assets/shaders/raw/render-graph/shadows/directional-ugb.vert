#version 460

#include "../common.glsl"
#include "../../view_info.glsl"

#extension GL_ARB_shader_draw_parameters: enable

layout(set = 1, binding = 0) uniform view_info {
    ViewInfo view;
} u_view_info;

layout(std430, set = 1, binding = 1) readonly buffer ugb_position {
    Position positions[];
} u_ugb_position;

layout(scalar, set = 1, binding = 2) readonly buffer object_buffer {
    RenderObject objects[];
} u_objects;

layout(std430, set = 1, binding = 3) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

void main() {
    const IndirectCommand command = u_commands.commands[gl_DrawIDARB];
    const RenderObject render_object = u_objects.objects[command.render_object];
    const mat4 model = render_object.model;
    const Position position = u_ugb_position.positions[render_object.position_index + gl_VertexIndex];
    gl_Position = u_view_info.view.view_projection * model * vec4(position.x, position.y, position.z, 1.0f);
}