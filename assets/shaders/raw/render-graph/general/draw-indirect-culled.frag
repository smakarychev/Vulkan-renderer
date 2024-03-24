#version 460

#include "common.shader_header"

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in flat uint vert_command_id;
layout(location = 1) in vec3 vertex_position;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec3 vertex_tangent;
layout(location = 4) in vec2 vertex_uv;

layout(std430, set = 1, binding = 6) readonly buffer command_buffer {
    IndirectCommand commands[];
} u_commands;

layout(std430, set = 1, binding = 7) readonly buffer triangle_buffer {
    uint8_t triangles[];
} u_triangles;

layout(location = 0) out vec4 out_color;

void main() {
    IndirectCommand command = u_commands.commands[vert_command_id];

    out_color = vec4((vertex_normal + 1.0f) / 2.0f, 1.0f);
}