#version 460

#include "common.glsl"

#extension GL_ARB_shader_draw_parameters: enable

layout(set = 1, binding = 0) uniform view_info {
    ViewInfo view;
} u_view_info;

layout(std430, set = 1, binding = 1) readonly buffer ugb_position {
    Position positions[];
} u_ugb_position;

layout(std430, set = 1, binding = 1) readonly buffer ugb_uv {
    UV uvs[];
} u_ugb_uv;

layout(scalar, set = 1, binding = 2) readonly buffer object_buffer {
    RenderObject objects[];
} u_objects;

layout(std430, set = 1, binding = 3) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(location = 0) out uint vertex_command_id;
layout(location = 1) out uint vertex_material_id;
layout(location = 2) out vec2 vertex_uv;

void main() {
    vertex_command_id = gl_DrawIDARB;
    const IndirectCommand command = u_commands.commands[vertex_command_id];
    const RenderObject render_object = u_objects.objects[command.render_object];
    vertex_material_id = render_object.material_id;

    const uint object_index = command.render_object;
    const mat4 model = u_objects.objects[object_index].model;

    const Position position = u_ugb_position.positions[render_object.position_index + gl_VertexIndex];
    const vec3 position_v = vec3(position.x, position.y, position.z);

    const UV uv = u_ugb_uv.uvs[render_object.uv_index + gl_VertexIndex];
    vertex_uv = vec2(uv.u, uv.v);

    gl_Position = u_view_info.view.view_projection * model * vec4(position_v, 1.0);
}