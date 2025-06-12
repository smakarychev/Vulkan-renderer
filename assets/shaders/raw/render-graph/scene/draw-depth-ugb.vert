#version 460

#include "../general/common.glsl"

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

layout(location = 0) out uint vertex_material_id;
layout(location = 1) out vec3 vertex_position;
layout(location = 2) out vec2 vertex_uv;

void main() {
    const IndirectCommand command = u_commands.commands[gl_DrawIDARB];
    const RenderObject render_object = u_objects.objects[command.render_object];

    vertex_material_id = render_object.material_id;

    const mat4 model = render_object.model;

    const Position position = u_ugb_position.positions[render_object.position_index + gl_VertexIndex];
    const vec4 position_v = model * vec4(position.x, position.y, position.z, 1.0f);
    vertex_position = position_v.xyz;

#ifdef ALPHA_TEST
    const UV uv = u_ugb_uv.uvs[render_object.uv_index + gl_VertexIndex];
    vertex_uv = vec2(uv.u, uv.v);
#endif
    gl_Position = u_view_info.view.view_projection * position_v;
}