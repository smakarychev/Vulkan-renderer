#version 460

#include "common.glsl"

#extension GL_ARB_shader_draw_parameters : enable

layout(set = 1, binding = 0) uniform camera_buffer {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
} u_camera;

layout(set = 1, binding = 1) readonly buffer position_buffer {
    Position positions[];
} u_positions;

layout(set = 1, binding = 2) readonly buffer normal_buffer {
    Normal normals[];
} u_normals;

layout(set = 1, binding = 3) readonly buffer tangent_buffer {
    Tangent tangents[];
} u_tangents;

layout(set = 1, binding = 4) readonly buffer uv_buffer {
    UV uvs[];
} u_uv;

layout(std430, set = 1, binding = 5) readonly buffer object_buffer {
    object_data objects[];
} u_objects;

layout(std430, set = 1, binding = 6) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;


layout(location = 0) out uint vertex_command_id;
layout(location = 1) out vec3 vertex_position;
layout(location = 2) out vec3 vertex_normal;
layout(location = 3) out vec3 vertex_tangent;
layout(location = 4) out vec2 vertex_uv;

void main() {
    VisibilityInfo visibility_info = unpack_visibility(gl_VertexIndex);
    const uint mask = (1 << 8) - 1;
    uint command_id = visibility_info.instance_id;
    uint index = visibility_info.triangle_id;

    IndirectCommand command = u_commands.commands[command_id];
    uint object_index = command.render_object;

    uint argument_index = command.vertexOffset + index;

    Position position = u_positions.positions[argument_index];
    vec3 position_v = vec3(position.x, position.y, position.z);
    
    Normal normal = u_normals.normals[argument_index];
    vec3 normal_v = vec3(normal.x, normal.y, normal.z);
    
    Tangent tangent = u_tangents.tangents[argument_index];
    vec3 tangent_v = vec3(tangent.x, tangent.y, tangent.z);

    UV uv = u_uv.uvs[argument_index];
    vec2 uv_v = vec2(uv.u, uv.v);

    gl_Position = u_camera.view_projection * u_objects.objects[object_index].model * vec4(position_v, 1.0);
    vertex_command_id = command_id;
    vertex_position = position_v;
    vertex_normal = normal_v;
    vertex_tangent = tangent_v;
    vertex_uv = uv_v;
}