#version 460

#include "common.glsl"

#extension GL_ARB_shader_draw_parameters: enable

layout(set = 1, binding = 0) uniform camera {
    CameraGPU camera;
} u_camera;

layout(std430, set = 1, binding = 1) readonly buffer positions_buffer {
    Position positions[];
} u_positions;

layout(std430, set = 1, binding = 2) readonly buffer normals_buffer {
    Normal normals[];
} u_normals;

layout(std430, set = 1, binding = 3) readonly buffer tangents_buffer {
    Tangent tangents[];
} u_tangents;

layout(std430, set = 1, binding = 4) readonly buffer uvs_buffer {
    UV uvs[];
} u_uv;

layout(std430, set = 1, binding = 5) readonly buffer object_buffer {
    object_data objects[];
} u_objects;

layout(std430, set = 1, binding = 6) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(location = 0) out uint vertex_object_index;
layout(location = 1) out vec3 vertex_position;
layout(location = 2) out vec3 vertex_normal;
layout(location = 3) out vec3 vertex_tangent;
layout(location = 4) out vec2 vertex_uv;

void main() {
    IndirectCommand command = u_commands.commands[gl_DrawIDARB];
    vertex_object_index = command.render_object;

    const mat4 model = u_objects.objects[vertex_object_index].model;

    const Position position = u_positions.positions[gl_VertexIndex];
    const vec4 position_v = u_camera.camera.view_projection * model * vec4(position.x, position.y, position.z, 1.0f);
    vertex_position = position_v.xyz;

    const Normal normal = u_normals.normals[gl_VertexIndex];
    vertex_normal = transpose(inverse(mat3(model))) * vec3(normal.x, normal.y, normal.z);
    const Tangent tangent = u_tangents.tangents[gl_VertexIndex];
    vertex_tangent = mat3(model) * vec3(tangent.x, tangent.y, tangent.z);
    const UV uv = u_uv.uvs[gl_VertexIndex];
    vertex_uv = vec2(uv.u, uv.v);

    gl_Position = position_v;
}